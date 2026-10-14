# DOA2 Ultimate bring-up

## Established baseline

The project has public source, synthetic tests, custody checks, a pinned
lifter and a native kernel/D3D8 runner. The actual selected game has been lifted;
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

## Graphics API contract

`d3d8_adapter.c` owns the whole 5849 CreateDevice boundary and uses the
committed sister-project D3D11 presenter. The plain device, frame and tile
models remain callable independently of interception. Creation accepts the
verified 720x480 linear BGRA8/D24S8 mode and two caller-provided color headers
plus depth. It validates their storage before creating the host device and
publishes the static guest device only after host creation succeeds.

The device has 5849's embedded 24-byte surface headers and owner/binding
references. Getters add references, Release preserves retained surfaces,
and default Swap rotates Data/Lock fields while keeping header addresses
fixed. Viewport extents are clamped; invalid origins stop. Tiles retain their
six-word API descriptors without exposing hardware. Clear and default Swap
submit semantic commands, with the shared guest-vblank pacing model.

`d3d8_state_adapter.c` binds gamma, ten transforms, point/directional light
records and active slots, and fog color. Gamma uses two identity-initialized
SDK shadow banks; Swap consumes the bank selected by preincrement frame
parity. Light-array growth repairs active pointers. HRESULT and void
calling conventions have separate synthetic ABI checks.

Alternate modes, parent resources, final resource destruction, offscreen
targets, partial viewport/rectangle clears, nonzero Swap flags, spot lights,
and additional resources/shaders/draws remain explicit limits or unbound
APIs. The borrowed-texture capture limitation inherited from the presenter
must be resolved before relying on queued non-linear texture uploads.
No title/menu acceptance is established by these model contracts.

The current explicit library frontier is `dsound:create-device-unbound`.
DirectSoundCreate needs a native audio-device replacement; its generated
factory reaches APU interrupts and MMIO. Keep that whole-library boundary
until its 5849 object and service contracts are verified. Private notes hold
actual graphics and boot observations.

## Remaining bring-up

1. Confirm the selected executable is the DOA2 portion using its launcher
   transition and code, and authenticate the disc revision. Preserve that
   evidence and the executable identity under `private/`.
2. Resolve the first observed unsupported startup instruction or service at
   its library boundary. Keep the whole-program lift as the temporary
   scaffold; fix translator defects in the lifter and regenerate.
3. Verify additional indirect targets and function bounds against the selected
   Ghidra program and private identity receipt when a run reaches them.
4. Rebind DirectSound and CRI services, then XAPI input/fibers. Verify native
   audio objects, movies, polling and worker scheduling against natural runs.
5. Bind additional D3D8 resources, shaders and draws through the semantic
   presenter. Verify title/menu entry and the first natural fight.

Online libraries and dashboard/update binaries are present on the disc.
The initial scope is offline DOA2; any online behavior needs its own design.

Record each observed frontier privately. A public model-test pass or a
completed lift alone is not a gameplay result.
