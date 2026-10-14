# DOA2 Ultimate bring-up

## Established baseline

The project has public source, synthetic tests, custody checks, a pinned
lifter and a native kernel/graphics/audio runner. The actual game has been lifted;
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

Memory statistics report the arena's available pages and reusable allocation
capacity, with image, pool, stack and virtual reservations accounted for.
Raw runtime allocations and alignment gaps stay consumed. Cache pages are
zero because this host has no separate guest cache-page allocator.

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
SetVertexShader stops at the API boundary before its SDK push-buffer path
can stall. Fixed-function vertex-format state, the remaining frame render
states, material and texture bindings are the next graphics work.

## Audio API contract

`dsound_api_adapter.c` owns the 5849 DirectSound device singleton and its
public interior handle. Native caller/buffer ownership replaces the SDK's
internal hardware voices while preserving visible reference results. The
shared XAudio2 backend owns copied PCM storage. Explicit mute keeps timing
available; host/device failures cannot report native readiness. Shutdown
requires the COM initialization owner thread.

The buffer adapter binds the 5849 descriptor and public handle layout to
the independent cursor model, PCM/Xbox ADPCM decoding and native output.
Owned and borrowed sample storage have separate lifetimes. Headroom changes
adjust effective volume; API argument cleanup is verified per entry.
The initial subset is non-3D buffers with synchronous polling/DoWork pumping.
The inherited clock drops output spans after a stall over 100 ms while
advancing its cursor. Native endpoint consumption has an opt-in host test;
synthetic API and backend tests remain suitable for public CI.

`dsound_effects_image.c` decodes the encrypted DSP programs; CPU parameter
states are plaintext. The parser owns its payload/maps, validates all spans
and rejects nonempty Y storage. Its 256-map budget bounds allocation and
overlap checks. Fixed decoder initialization comes from the authenticated
executable at runtime. Synthetic vectors use independent initialization;
no retail programs, keys or state are included in public source.

`dsound_effects_adapter.c` publishes owned guest-readable descriptor, state
and scratch allocations. Failed parsing/allocation leaves the old image
intact. Immediate state writes update the published view; deferred writes
stay in shadow storage and flush with a dirty listener commit. Direct CPU
state edits survive unless explicitly overwritten by a deferred update.
This initial adapter requires an assigned reverb role. Unassigned roles are
rejected before publication, preserving the current native graph and image.

Native I3DL2 reverb runs on a stereo XAudio2 submix, using Microsoft's
[parameter converter](https://learn.microsoft.com/en-us/windows/win32/api/xaudio2fx/nf-xaudio2fx-reverbconverti3dl2tonative).
The source keeps separate dry/wet matrices, per-route volumes and per-bin
headroom. Front stereo, wet bin 10 and muted other speaker routes are bound.
Recorded impulse tests establish actual wet processing, not Xbox bit parity;
the built-in converter ignores room rolloff. DSP programs are not executed.

Recording sends contain decimation, phase and scratch/DMA transfer semantics;
ordinary SourceVoice resampling is insufficient. Those sends and 3D crosstalk
remain guarded, as do packet streams, scheduled StopEx, additional play flags
and unsupported DSP processing. This is an initial native FX subset.
Private notes hold actual graphics/audio startup and boot observations.

The 5849 GetTickCount boundary reads shared monotonic time and offers bounded
cooperative scheduler polling. D3D BlockUntilVerticalBlank and its two CRI
tail aliases use the native shared-vblank service. These platform seams let
the existing worker modules run; they do not force middleware load status.

## Remaining bring-up

1. Confirm the selected executable is the DOA2 portion using its launcher
   transition and code, and authenticate the disc revision. Preserve that
   evidence and the executable identity under `private/`.
2. Resolve the first observed unsupported startup instruction or service at
   its library boundary. Keep the whole-program lift as the temporary
   scaffold; fix translator defects in the lifter and regenerate.
3. Verify additional indirect targets and function bounds against the selected
   Ghidra program and private identity receipt when a run reaches them.
4. Complete native recording/crosstalk routing and CRI services, then XAPI
   input/fibers. Verify native audio, movies, polling and worker scheduling
   against natural runs.
5. Bind additional D3D8 resources, shaders and draws through the semantic
   presenter. Verify title/menu entry and the first natural fight.

Online libraries and dashboard/update binaries are present on the disc.
The initial scope is offline DOA2; any online behavior needs its own design.
The verified XNetStartup boundary returns the native offline policy error
WSASYSNOTREADY without creating a console network stack. The game handles
its helper's failure; this seam does not report successful network startup.
PHY initialization reports hardware unavailable, and link queries return
disconnected flags, including after the game's failed network startup.

Record each observed frontier privately. A public model-test pass or a
completed lift alone is not a gameplay result.
