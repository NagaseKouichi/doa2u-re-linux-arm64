# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic source authentication and target-custody checks."""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from build_game import program_manifest, selected_xbe
from extract_iso import sha256


class BuildTests(unittest.TestCase):
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


if __name__ == "__main__":
    unittest.main()
