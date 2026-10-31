# SPDX-License-Identifier: GPL-3.0-or-later
import io
from pathlib import Path
import tempfile
import unittest
from unittest import mock
import zipfile

import update


def archive(files):
    data = io.BytesIO()
    with zipfile.ZipFile(data, "w") as zipped:
        for name, content in files.items():
            zipped.writestr(name, content)
    return zipfile.ZipFile(data)


class UpdateTest(unittest.TestCase):
    def test_arm64_uses_fork_release_endpoint(self):
        response = io.StringIO('{"tag_name":"v0.1.13","assets":[]}')
        with mock.patch.object(update.sys, "platform", "linux"), \
                mock.patch("platform.machine", return_value="aarch64"), \
                mock.patch.object(update, "fetch", return_value=response) as fetch:
            self.assertEqual(update.newer("0.1.12")["tag_name"], "v0.1.13")
            fetch.assert_called_once_with("https://api.github.com/repos/NagaseKouichi/doa2u-re-linux-arm64/releases/latest")

    def test_arm64_only_selects_explicit_arm64_zip(self):
        assets = [{"name": "DOA2U-0.1.13-Linux.zip"},
                  {"name": "DOA2U-0.1.13-Linux-ARM64.zip"}]
        for machine in ("aarch64", "arm64"):
            with mock.patch.object(update.sys, "platform", "linux"), \
                    mock.patch("platform.machine", return_value=machine):
                self.assertEqual(update.pick_asset(assets), assets[1])
                with self.assertRaises(ValueError):
                    update.pick_asset(assets[:1])

    def test_arm64_check_is_enabled(self):
        with mock.patch.object(update.sys, "platform", "linux"), \
                mock.patch("platform.machine", return_value="aarch64"), \
                mock.patch.object(update, "installed", return_value="0.1.12"), \
                mock.patch.object(update, "newer", return_value=None) as newer:
            self.assertEqual(update.main(["check"]), 0)
            newer.assert_called_once_with("0.1.12")

    def test_versions_compare_numerically(self):
        self.assertGreater(update.number("v0.1.10"), update.number("0.1.9-Alpha"))
        self.assertEqual(update.number("0.1.10"), update.number("0.1.10-Alpha"))

    def test_asset_matches_the_platform(self):
        assets = [{"name": "DOA2U-1-Alpha-Linux.zip"}, {"name": "DOA2U-1-Alpha-macOS.zip"}, {"name": "DOA2U-1-Alpha.zip"}]
        for suffix, expected in (("", "DOA2U-1-Alpha.zip"), ("-macOS", "DOA2U-1-Alpha-macOS.zip"), ("-Linux", "DOA2U-1-Alpha-Linux.zip")):
            with mock.patch.object(update, "SUFFIX", suffix):
                self.assertEqual(update.pick_asset(assets)["name"], expected)

    def test_unpack_keeps_private_files_and_writes_version_last(self):
        with tempfile.TemporaryDirectory() as folder, mock.patch.object(update, "ROOT", Path(folder).resolve()):
            root = update.ROOT
            (root / "private").mkdir()
            (root / "private" / "launcher.json").write_text("mine")
            (root / "Launcher.exe").write_text("old")
            update.unpack(archive({"DOA2U-2/VERSION": "2", "DOA2U-2/Launcher.exe": "new", "DOA2U-2/tools/a.py": "a",
                                   "DOA2U-2/private/launcher.json": "release", "DOA2U-2/private/photo.jpg": "photo"}))
            self.assertEqual((root / "Launcher.exe").read_text(), "new")
            self.assertEqual((root / "Launcher.exe.old").read_text(), "old")
            self.assertEqual((root / "tools" / "a.py").read_text(), "a")
            self.assertEqual((root / "private" / "launcher.json").read_text(), "mine")
            self.assertEqual((root / "private" / "photo.jpg").read_text(), "photo")
            # A failed write puts the running exe back and leaves VERSION, which goes last, for a retry.
            real_write = Path.write_bytes

            def fail_on_exe(path, data):
                if path.name == "Launcher.exe":
                    raise OSError("disk full")
                return real_write(path, data)

            with mock.patch.object(Path, "write_bytes", fail_on_exe):
                with self.assertRaises(OSError):
                    update.unpack(archive({"DOA2U-3/VERSION": "3", "DOA2U-3/tools/a.py": "b", "DOA2U-3/Launcher.exe": "x"}))
            self.assertEqual((root / "tools" / "a.py").read_text(), "b")
            self.assertEqual((root / "VERSION").read_text(), "2")
            self.assertEqual((root / "Launcher.exe").read_text(), "new")

    def test_unsafe_entry_stops_before_any_write(self):
        with tempfile.TemporaryDirectory() as folder, mock.patch.object(update, "ROOT", Path(folder).resolve()):
            with self.assertRaises(ValueError):
                update.unpack(archive({"DOA2U-2/tools/a.py": "a", "DOA2U-2/../../evil": "x"}))
            self.assertFalse((update.ROOT / "tools").exists())


if __name__ == "__main__":
    unittest.main()
