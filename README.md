# Dead or Alive 2 Ultimate Native PC Port

A starter project for a native Windows port of the Xbox release of Dead or
Alive 2 Ultimate, followed by replacement of generated game logic with readable,
hand-written source. Sister projects: [DOA3](https://github.com/NoRain211/doa3-re)
and [DOAXBV](https://github.com/NoRain211/doaxbv-re).

This repository contains no game files. Original project source and
documentation are GPL-3.0-or-later; see [LICENSE](LICENSE) and [NOTICE](NOTICE).

## Status

Initialized for the USA-labelled Ultimate Disc 2 image. The private import has
been checked for consistency; its retail revision has not been authenticated
against an independent disc catalog. The disc's small launcher is separate
from its main game executable. Executable paths, hashes and inventory belong
in `private/`; the selected target is recorded in `private/target.json`.

The main executable records XDK, D3D8 and DirectSound build 5849. DOA3 uses
3911/3925/3936 and DOAXBV uses 4928, so their library address bindings cannot
be reused unchanged.

The starter builds synthetic tests for shared models, audio helpers, input
pulses, controller settings, save transactions and frame pacing. No game
runner or whole-program lift exists yet. Passing these tests does not establish
DOA2U boot, rendering, sound or gameplay compatibility.

Read [reuse](docs/reuse.md) for the source provenance and what can be brought
over next, and [bring-up](docs/bring-up.md) for the next implementation steps.

## Build the public tests

No game files or lifter checkout are needed:

```powershell
python -m unittest discover -s tools -p "test_*.py"
python tools/public_export.py verify --require-public-tree
cmake -S recomp-runtime -B build/recomp-runtime
cmake --build build/recomp-runtime --config Debug
ctest --test-dir build/recomp-runtime -C Debug --output-on-failure
```

## Import another local disc

Use XboxDev [extract-xiso](https://github.com/XboxDev/extract-xiso) on PATH or
pass its executable with `--extractor`. The importer retains logs and a hash
receipt under `private/`, refuses existing output directories, and verifies
that the original ISO did not change.

```powershell
python tools/extract_iso.py <your-disc.iso> --output private/new-import
```

The source ISO remains in its original location. Extract one disc once and
reuse it; future play folders should use hardlinks instead of copying it.

## Layout

| Path | Purpose |
| --- | --- |
| `recomp-runtime/` | Selected shared source, synthetic tests, empty manual lookup |
| `tools/` | Disc import, XBE inspection and public custody checks |
| `tools/xboxrecomp` | Pinned lifter submodule; no generated game output |
| `docs/` | Reuse assessment and bring-up plan |
| `private/` | Ignored imports, target receipt, generated output and evidence |
