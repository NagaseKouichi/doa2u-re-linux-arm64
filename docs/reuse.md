# Sister-project reuse

The starter takes committed public source from DOA3 at
`69cb910a13a737534fe2272cffd6211b1edf8d8e`. DOAXBV was inspected at
`1cdbd62167d4a0825f73895604311a2c761164b2`. Local uncommitted changes,
private inputs, generated code and caches were excluded.

## Already brought over

| Area | Source and coverage |
| --- | --- |
| Shared models | APU, OHCI, fibers, directory enumeration, symbolic links, CRI service lifecycle and D3D frame commands; existing synthetic suites |
| Audio helpers | Xbox ADPCM decoder and DirectSound HRTF; existing standalone tests |
| Movie helper | Planar color conversion; existing standalone tests |
| Input | Scripted pulse source and persistent controller settings; existing tests |
| Saves | Save transaction journal, recovery and validation; existing tests |
| Timing | Guest clock and Windows vblank pacing, including the existing unpaced test |
| Tooling | Verified ISO extraction, synthetic importer tests, XBE metadata inspection and public export checker |

`runtime.h` and the input/presenter model headers are retained as dependencies
of these tests. Their presence does not supply a complete runtime or renderer.
`program_manual.c` starts with no guest bindings.

## Good next candidates, with work required

| Candidate | Reusable part | DOA2U work |
| --- | --- | --- |
| Kernel and startup | Ordinal dispatch, files, memory, crypto, threads and cooperative scheduler | Verify exports, launch behavior, TLS and memory allocation against the actual image |
| D3D8 | Creation/draw/state/texture/shader models, semantic presenter and D3D11 backend | Find 5849 functions, verify device/resource layouts and ABI, remove title-specific presentation assumptions |
| Input and fibers | SDL host, controller model and native fibers | Rebind XAPI/XPP entry points and verify polling, vibration and stack transfer |
| Audio | PCM/XAudio2 output, ADPCM and service models | Rebind DirectSound 5849 objects and verify stream timing, loops, 3D state and CRI worker behavior |
| Movies | CRI lifecycle and color conversion | Check this revision's media format, decoder and worker contract before enabling playback |
| Build pipeline | Extraction, hashing, authenticated manifests and lifter orchestration | Supply DOA2U function discovery, ABI and manual-binding recipe; select the game rather than its launcher |

DOAXBV is the source for broader host/platform support. DOA3 supplies the
closer fighting-game investigation and some newer rendering work. Neither is
a drop-in runner for this title.

## Boundaries that prevent blind copying

- SDK builds differ: DOA2U records 5849, DOAXBV 4928, and DOA3
  XAPI 3911 / D3D8 3925 / DirectSound 3936.
- In DOA3, omitting `RECOMP_DOAXBV_BINDINGS` selects DOA3 bindings. It does
  not disable interception. DOAXBV's lookup also enables its own addresses.
- Both runners contain title-specific globals, startup shortcuts and
  presentation behavior. Their launchers, recipes and generated forwarders
  need replacement rather than a title string change.
- The selected DOA2U image exceeds DOA3's reserved kernel-data and fixed
  heap-start boundaries. The allocator and memory layout were therefore
  excluded. Establish a non-overlapping layout before any game execution.
- Some tests named `*_model_test` also execute address adapters. Only the
  independent suites were retained.
- DOA3 collision logic and DOAXBV vacation, animation and idle-mode hooks
  remain in their respective projects. Similar engine ancestry does not
  establish identical game-owned structures.

Shared decisions stay in DOAXBV's [custody](https://github.com/NoRain211/doaxbv-re/issues/2),
[presenter](https://github.com/NoRain211/doaxbv-re/issues/6), and
[audio/movie](https://github.com/NoRain211/doaxbv-re/issues/8) issues.
