# SPDX-License-Identifier: GPL-3.0-or-later
"""Find the pinned llvm-mingw compiler, downloading it into tools/llvm-mingw when missing."""
# Adapted from committed DOAXBV tools/build_prerequisites.py at 5761e89.
import os
from pathlib import Path
import shutil
import tempfile
import urllib.error
import urllib.request
import zipfile

from extract_iso import sha256

LLVM_MINGW_URL = ("https://github.com/mstorsjo/llvm-mingw/releases/download/20261006/"
                  "llvm-mingw-20261006-ucrt-x86_64.zip")
LLVM_MINGW_SHA256 = "317492c456aa27ee607a5919f1d2d38dcdc1112516a24d0bf4b00d078f52d17a"
LLVM_MINGW = Path("tools/llvm-mingw")


def install_llvm_mingw(root):
    target = root / LLVM_MINGW
    if target.exists():
        raise ValueError(f"{target} exists but has no bin/clang.exe. Delete it and rerun the build.")
    private = root / "private"
    private.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="llvm-mingw-", dir=private) as folder:
        archive = Path(folder) / "llvm-mingw.zip"
        print("Downloading the llvm-mingw compiler (190 MB)...", flush=True)
        try:
            with urllib.request.urlopen(LLVM_MINGW_URL, timeout=60) as response, archive.open("xb") as file:
                shutil.copyfileobj(response, file)
        except (OSError, urllib.error.URLError) as error:
            raise ValueError(f"Could not download llvm-mingw: {error}. Retry when online.") from error
        if sha256(archive) != LLVM_MINGW_SHA256:
            raise ValueError("The llvm-mingw download failed its SHA-256 check. Rerun the build.")
        print("Extracting llvm-mingw...", flush=True)
        staging = Path(folder) / "extract"
        with zipfile.ZipFile(archive) as contents:
            contents.extractall(staging)
        (top,) = staging.iterdir()  # the release ZIP holds one versioned folder
        target.parent.mkdir(parents=True, exist_ok=True)
        top.replace(target)


def llvm_mingw(root):
    """Return clang.exe and put its folder, which also holds mingw32-make, first on PATH."""
    clang = root / LLVM_MINGW / "bin/clang.exe"
    if not clang.is_file():
        install_llvm_mingw(root)
        if not clang.is_file():
            raise ValueError(f"llvm-mingw was extracted, but {clang} is missing; extract {LLVM_MINGW_URL} there.")
    path = os.environ.get("PATH", "").split(os.pathsep)
    os.environ["PATH"] = os.pathsep.join([str(clang.parent)] + [p for p in path if p != str(clang.parent)])
    return clang
