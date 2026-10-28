# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic source authentication and target-custody checks."""
from contextlib import ExitStack
import json
import sys
from types import SimpleNamespace
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from build_game import (STEPS, build_fraction, import_disc, lifter_revision, merge_functions,
                        program_manifest, selected_xbe, use_windows_newlines, write_functions)
from extract_iso import sha256


class CrossBuildTests(unittest.TestCase):
    def setUp(self):
        self.stack = ExitStack()
        self.addCleanup(self.stack.close)
        self.root = Path(self.stack.enter_context(tempfile.TemporaryDirectory()))
        self.work = self.root / "private/program"
        generated = self.work / "generated"
        generated.mkdir(parents=True)
        for name in ("recomp_0000.c", "recomp_dispatch.c", "recomp_funcs.h"):
            (generated / name).write_text("/* synthetic */\n")
        xbe = self.root / "private/synthetic.input"
        xbe.write_bytes(b"synthetic input")
        (self.root / "private/target.json").write_text(json.dumps(dict(
            xbe_path="private/synthetic.input", xbe_sha256=sha256(xbe))))
        shards = self.root / "shards"
        shards.mkdir()
        (shards / "00.json").write_text("[]")
        functions = self.work / "functions.json"
        functions.write_text("[]")
        manifest, ebp = program_manifest(generated)
        self.receipt = dict(recipe=dict(xbe_sha256=sha256(xbe), lifter_commit="a" * 40,
                                       capstone="synthetic", functions_sha256=sha256(functions)),
                            manifest_sha256=manifest, ebp_count=ebp)
        self.receipt_path = self.work / "receipt.json"
        self.receipt_path.write_text(json.dumps(self.receipt))
        self.toolchain = self.root / "cross toolchain.cmake"
        self.toolchain.write_text("# synthetic toolchain\n")
        self.calls = []
        self.stack.enter_context(patch("build_game.ROOT", self.root))
        self.stack.enter_context(patch("build_game.FUNCTIONS", shards))
        self.stack.enter_context(patch("build_game.sys.platform", "linux"))
        self.stack.enter_context(patch.dict(sys.modules, capstone=SimpleNamespace(__version__="synthetic")))
        self.stack.enter_context(patch("build_game.lifter_revision", return_value="a" * 40))
        self.stack.enter_context(patch("build_game.Progress.run", side_effect=self.record_command))

    def record_command(self, args, log_path, cwd=None):
        self.calls.append(args)
        if args[:2] == ["cmake", "--build"]:
            build_dir = Path(args[2])
            build_dir.mkdir(parents=True, exist_ok=True)
            (build_dir / "recomp_program_runner").write_bytes(b"synthetic runner")

    def assert_build(self, build_dir, toolchain=None):
        configure, compile_command = self.calls
        self.assertEqual(configure[configure.index("-B") + 1], build_dir)
        self.assertEqual(compile_command[2], build_dir)
        self.assertIn("Ninja", configure)
        self.assertIn("-DRECOMP_FETCH_SDL3=ON", configure)
        for option, value in (("RECOMP_XBE_SHA256", self.receipt["recipe"]["xbe_sha256"]),
                              ("RECOMP_PROGRAM_MANIFEST_SHA256", self.receipt["manifest_sha256"]),
                              ("RECOMP_PROGRAM_EBP_EXPECTED", self.receipt["ebp_count"])):
            self.assertIn(f"-D{option}={value}", configure)
        if toolchain is not None:
            self.assertIn(f"-DCMAKE_TOOLCHAIN_FILE={toolchain.resolve().as_posix()}", configure)
        else:
            self.assertFalse(any(str(arg).startswith("-DCMAKE_TOOLCHAIN_FILE=") for arg in configure))
        receipt = json.loads(self.receipt_path.read_text())
        self.assertEqual(receipt["recipe"], self.receipt["recipe"])
        self.assertEqual(receipt["runner_sha256"], sha256(build_dir / "recomp_program_runner"))

    def test_native_build_keeps_default_directory(self):
        import build_game
        build_game.build(build_only=True)
        self.assert_build(self.root / "build/recomp-program")

    def test_toolchain_uses_separate_default_directory(self):
        import build_game
        build_game.build(build_only=True, cmake_toolchain=self.toolchain)
        self.assert_build(self.root / "build/recomp-program-cross", self.toolchain)

    def test_explicit_directory_is_used_with_or_without_toolchain(self):
        import build_game
        for toolchain in (None, self.toolchain):
            with self.subTest(toolchain=toolchain):
                self.calls.clear()
                directory = self.root / "custom build"
                build_game.build(build_only=True, cmake_toolchain=toolchain, build_dir=directory)
                self.assert_build(directory, toolchain)

    def test_relative_toolchain_is_resolved_from_working_directory(self):
        import build_game
        import os
        cwd = Path.cwd()
        self.addCleanup(os.chdir, cwd)
        os.chdir(self.root)
        build_game.build(build_only=True, cmake_toolchain=Path(self.toolchain.name))
        self.assert_build(self.root / "build/recomp-program-cross", self.toolchain)

    def test_cross_build_still_rejects_tampered_generated_sources(self):
        import build_game
        (self.work / "generated/recomp_funcs.h").write_text("changed")
        with self.assertRaisesRegex(ValueError, "authenticated receipt"):
            build_game.build(build_only=True, cmake_toolchain=self.toolchain)
        self.assertEqual(self.calls, [])

    def test_missing_toolchain_is_rejected_before_build(self):
        import build_game
        with self.assertRaisesRegex(ValueError, "toolchain"):
            build_game.build(build_only=True, cmake_toolchain=self.root / "missing.cmake")
        self.assertEqual(self.calls, [])

    def test_windows_rejects_cross_toolchain_before_fetching_compiler(self):
        import build_game
        with patch("build_game.sys.platform", "win32"), patch("build_game.llvm_mingw") as compiler:
            with self.assertRaisesRegex(ValueError, "Windows"):
                build_game.build(build_only=True, cmake_toolchain=self.toolchain)
        compiler.assert_not_called()

    def test_runner_path_accepts_explicit_directory_without_changing_native_defaults(self):
        from build_game import runner_path
        for platform, default, name in (("linux", "recomp-program", "recomp_program_runner"),
                                         ("win32", "recomp-program-mingw", "recomp_program_runner.exe")):
            with self.subTest(platform=platform), patch("build_game.sys.platform", platform):
                self.assertEqual(runner_path(self.root), self.root / "build" / default / name)
                self.assertEqual(runner_path(self.root, build_dir=self.root / "custom"),
                                 self.root / "custom" / name)

    def test_cli_passes_build_options_as_paths(self):
        import build_game
        directory = self.root / "custom"
        argv = ["build_game.py", "--build-only", "--cmake-toolchain", str(self.toolchain),
                "--build-dir", str(directory)]
        with patch.object(sys, "argv", argv), patch("build_game.build") as build:
            self.assertEqual(build_game.main(), 0)
        self.assertEqual(build.call_args.kwargs["cmake_toolchain"], self.toolchain)
        self.assertEqual(build.call_args.kwargs["build_dir"], directory)


class BuildTests(unittest.TestCase):
    def test_function_shards_round_trip_without_call_edges(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            functions = [{"start": hex(i), "calls_to": [], "called_by": ["0x1"]} for i in range(2500)]
            (root / "functions.json").write_text(json.dumps(functions))
            (root / "shards").mkdir()
            (root / "shards/99.json").write_text("[]")
            with patch("build_game.FUNCTIONS", root / "shards"):
                write_functions(root)
                merge_functions(root / "merged.json")
            self.assertEqual(sorted(p.name for p in (root / "shards").iterdir()), ["00.json", "01.json", "02.json"])
            self.assertEqual(json.loads((root / "merged.json").read_text()), [{"start": hex(i)} for i in range(2500)])

    def test_progress_counts_finished_steps_then_output_or_time(self):
        plan = list(STEPS)
        total = sum(seconds for seconds, _ in STEPS.values())
        self.assertEqual(build_fraction(plan, 0, 0, 0), 0)
        # Half the lift's output after the parser and disassembly.
        self.assertAlmostEqual(build_fraction(plan, 2, 27, 0), (5 + 1440 + 315 * 27 / 55) / total)
        # A silent, slow disassembly moves by time but stops short of its end.
        self.assertAlmostEqual(build_fraction(plan, 1, 0, 10**6), (5 + 1440 * 0.95) / total)
        self.assertAlmostEqual(build_fraction(plan, 1, 500, 0), (5 + 1440 * 0.99) / total)
        self.assertEqual(build_fraction(plan, len(plan), 0, 0), 1)
        self.assertEqual(build_fraction(["configure", "build"], 1, 0, 0), 30 / 275)
        # The compiler's up-front file list does not move the bar.
        self.assertEqual(build_fraction(["build"], 0, 109, 0), 0)
        self.assertAlmostEqual(build_fraction(["build"], 0, 0, 10**6), 0.95)

    def test_manifest_authenticates_members_and_contents(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / "recomp_0000.c").write_text("void synthetic(void) {\n    uint32_t ebp;\n}\n")
            (root / "recomp_dispatch.c").write_text("/* synthetic dispatcher */\n")
            (root / "recomp_funcs.h").write_text("void synthetic(void);\n")
            before, count = program_manifest(root)
            self.assertEqual(count, 1)
            (root / "recomp_funcs.h").write_text("void changed(void);\n")
            self.assertNotEqual(before, program_manifest(root)[0])
            (root / "recomp_0002.c").write_text("/* gap */\n")
            with self.assertRaises(ValueError):
                program_manifest(root)

    def test_generated_files_get_the_windows_line_endings_the_manifest_pins(self):
        with tempfile.TemporaryDirectory() as folder:
            generated = Path(folder)
            (generated / "recomp_0000.c").write_bytes(b"a\nb\r\nc\n")
            with patch("build_game.sys.platform", "darwin"):
                use_windows_newlines(generated)
            self.assertEqual((generated / "recomp_0000.c").read_bytes(), b"a\r\nb\r\nc\r\n")
            (generated / "recomp_0000.c").write_bytes(b"a\n")
            with patch("build_game.sys.platform", "win32"):
                use_windows_newlines(generated)
            self.assertEqual((generated / "recomp_0000.c").read_bytes(), b"a\n")

    def test_target_hash_and_private_boundary(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / "private").mkdir()
            target = root / "private/target.json"
            xbe = root / "private/synthetic.input"
            xbe.write_bytes(b"synthetic input")
            identity = dict(xbe_path="private/synthetic.input", xbe_sha256=sha256(xbe))
            target.write_text(json.dumps(identity))
            with patch("build_game.ROOT", root):
                self.assertEqual(selected_xbe(), xbe.resolve())
                xbe.write_bytes(b"changed")
                with self.assertRaises(ValueError):
                    selected_xbe()
                outside = root / "outside.input"
                outside.write_bytes(b"outside")
                identity.update(xbe_path="private/../outside.input", xbe_sha256=sha256(outside))
                target.write_text(json.dumps(identity))
                with self.assertRaises(ValueError):
                    selected_xbe()

    def test_import_reuses_matching_extraction_and_selects_by_hash(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            disc = root / "private/imported-disc/disc"
            disc.mkdir(parents=True)
            iso = root / "synthetic.iso"
            iso.write_bytes(b"synthetic image")
            (root / "private/imported-disc/receipt.json").write_text(json.dumps(dict(iso_sha256=sha256(iso))))
            (disc / "a.xbe").write_bytes(b"launcher")
            (disc / "b.xbe").write_bytes(b"game")
            (root / "private/target.json").write_text(json.dumps(dict(note="kept")))
            with patch("build_game.ROOT", root), patch("build_game.XBE_SHA256", sha256(disc / "b.xbe")), \
                    patch("build_game.extract") as extract:
                import_disc(iso)
                extract.assert_not_called()
                target = json.loads((root / "private/target.json").read_text())
                self.assertEqual(target["xbe_path"], "private/imported-disc/disc/b.xbe")
                self.assertEqual(target["note"], "kept")
                self.assertEqual(selected_xbe(), (disc / "b.xbe").resolve())
                iso.write_bytes(b"another image")
                with self.assertRaises(ValueError):
                    import_disc(iso)
            with patch("build_game.ROOT", root), patch("build_game.XBE_SHA256", "0" * 64):
                iso.write_bytes(b"synthetic image")
                with self.assertRaises(ValueError):
                    import_disc(iso)

    def test_packaged_lifter_uses_the_export_pin(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / "lifter").mkdir()
            links = [dict(path="tools/xboxrecomp", commit="a" * 40)]
            (root / "public-export.json").write_text(json.dumps(dict(gitlinks=links)))
            with patch("build_game.ROOT", root), patch("build_game.LIFTER", root / "lifter"):
                self.assertEqual(lifter_revision(), "a" * 40)


if __name__ == "__main__":
    unittest.main()
