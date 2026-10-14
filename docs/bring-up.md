# DOA2 Ultimate bring-up

## Established baseline

The project has public source, synthetic tests, custody checks, a pinned
lifter and a kernel-only runner. The actual selected game has been lifted;
its generated source and receipt remain private. Library API replacements
and game compatibility remain bring-up work.

Ultimate's launcher and main game are separate executables. Read the ignored
`private/target.json` to obtain the selected game path and its hashes; do not
assume the disc's default executable is the game.

The ISO hash establishes import consistency, not independent authentication
of a retail disc revision. Keep the selected revision provisional until its
identity has been checked against a trusted independent reference.

## Memory and startup

The allocation arena starts at the loaded image's page-aligned end and stops
at `0x03F00000`. The remaining guest RAM holds the bootstrap stack, kernel
exports, PCR and TLS. PCR occupies `0x03FF0800` through `0x03FF0BFF`;
generated FS reads and native thread snapshots use that dedicated block.
Physical page zero remains available to the SDK startup pool and cannot
overwrite PCR. Heap and contiguous allocations grow toward each other and
reject overlap. The loader checks sections, source digests, stack/TLS bounds
and reserved ranges before running code.

Bootstrap TLS data is aligned to 16 bytes, with its pointer slot included in
the stack-base calculation used by XAPI's negative TLS index. The selected
entry creates the startup worker and returns; the host then schedules that
worker. Closing its handle invalidates external use while retaining the
worker until it exits.

The XAPI kernel-compatibility entry is a temporary whole-library replacement:
the native host owns its kernel's executable lifetime, so console kernel
patching and INIT/LDT registration are unnecessary. Its no-argument calling
convention is tested; it consumes the return slot and leaves game globals
untouched. SDK initializer-table callbacks are supplied through the public
seed recipe rather than replaced by success stubs.

`tools/build_game.py` authenticates the selected XBE, lifter revision,
generated source manifest and final runner. CMake rejects partial or wrong
authentication, chunk gaps and uninitialized generated EBP prologues. Missing
function bodies, unsupported imports and untranslated instructions stop
explicitly.

Use `tools/run_game.py` for a bounded probe. It hardlinks the disc files,
preserves saves and logs the runner identity. Private notes hold the observed
frontier; a timeout by itself does not establish gameplay progress.

The current explicit library frontier is `d3d8:create-device-unbound`.
Implement the verified 5849 CreateDevice boundary and semantic presenter next;
the raw GPU path below it remains outside the native runtime.

## Remaining bring-up

1. Confirm the selected executable is the DOA2 portion using its launcher
   transition and code, and authenticate the disc revision. Preserve that
   evidence and the executable identity under `private/`.
2. Resolve the first observed unsupported startup instruction or service at
   its library boundary. Keep the whole-program lift as the temporary
   scaffold; fix translator defects in the lifter and regenerate.
3. Verify additional indirect targets and function bounds against the selected
   Ghidra program and private identity receipt when a run reaches them.
4. Rebind XAPI input/fibers and D3D8 5849 creation, clear and present. Then
   validate resources, shaders and draws through the semantic presenter.
5. Rebind DirectSound and CRI services. Verify movies, sound and worker
   scheduling, then test title/menu entry and the first natural fight.

Online libraries and dashboard/update binaries are present on the disc.
The initial scope is offline DOA2; any online behavior needs its own design.

Record each observed frontier privately. A public model-test pass or a
completed lift alone is not a gameplay result.
