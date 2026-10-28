# Linux AArch64 bring-up

[中文说明](linux-arm64-CN.md)

This branch adds x86-64-hosted Linux AArch64 cross builds. It does not yet
claim that the game works on an ARM64 handheld. The public runtime and settings
launcher can be built without a disc; the game runner still requires your own
supported USA Disc 2 and the upstream authenticated lift. No game-derived
source or assets are distributed.

## Isolated cross environment

From the repository root, with Docker available:

```sh
docker build --network=host -t doa2u-arm64-cross:local tools/arm64
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD:/work" \
  doa2u-arm64-cross:local sh -c '
    RECOMP_QEMU_AARCH64=/usr/bin/qemu-aarch64 cmake \
      -S recomp-runtime -B private/arm64-build/runtime -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=/work/cmake/linux-aarch64.cmake \
      -DRECOMP_FETCH_SDL3=ON &&
    cmake --build private/arm64-build/runtime --parallel 4 &&
    ctest --test-dir private/arm64-build/runtime --output-on-failure \
      -E d3d-presenter'
```

The image uses Ubuntu 24.04's GCC 13 cross compiler, ARM64 SDL dependencies,
shaderc and Mesa. SDL itself is fetched at the upstream hash-pinned version.
The distro image/packages are not a fully pinned reproducible supply chain.
No packages are installed on the host or handheld. A real desktop build must
have X11/Wayland and audio backends: do not ship an SDL dummy-only test build.
QEMU is used only for test execution, not for compilation or playing the game.
The toolchain checks ARM64 compiler architecture in `tools/test_arm64_toolchain.py`.

The optional `RECOMP_TARGET_PREFIX` CMake path adds a target dependency prefix.
`RECOMP_QEMU_AARCH64` opts into the CTest emulator; omit it for native-only tests
on a device. Host tools remain native, pkg-config searches ARM64 packages, and
x86-64 library prefixes are excluded. The produced ELF machine must be AArch64.

For an additional *software* GPU check in the same container:

```sh
SDL_VIDEO_DRIVER=offscreen RECOMP_TEST_SOFTWARE_VULKAN=1 \
  ctest --test-dir private/arm64-build/runtime --output-on-failure -R d3d-presenter
```

This is not a physical GPU, display, audio endpoint, or gamepad acceptance test.
The presenter test's upstream 60-second timeout may be too short under QEMU.

## Once the disc is available

Keep the ISO in its original location. First run the upstream extraction and
`--lift-only` workflow on the host with its pinned lifter, Capstone and extractor
(see [building](building.md)). Generated files remain under `private/`. Then:

```sh
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD:/work" \
  doa2u-arm64-cross:local python3 tools/build_game.py --build-only \
  --cmake-toolchain cmake/linux-aarch64.cmake \
  --build-dir build/recomp-program-arm64
```

`--build-only` still reads the Python Capstone version as part of its recipe;
use the same pinned Capstone as the lift. The image includes
`capstone==5.0.7` and `pillow==12.2.0` in its own venv. This post-disc workflow cannot be validated without
the user's game. Do not disable the XBE, generated-source or runner hashes.
Only one runner hash is stored in the upstream receipt: rebuilding another
architecture replaces it. Restore/rebuild the matching receipt rather than
bypassing validation when switching architectures.

On the target, with matching ARM64 SDL3/shaderc and Vulkan driver available:

```sh
./launcher.sh --build-dir build/recomp-program-arm64
```

Native build defaults and existing launch commands are unchanged. A cross
build without `--build-dir` uses `build/recomp-program-cross`; pass that same
path when launching. Relative build directories are relative to this repo;
relative toolchain paths are resolved from the invoking working directory.

For Fedora immutable handheld deployment, stage missing runtime dependencies
in a user-directory bundle, never in `/usr` or `/lib`. GPU driver libraries must
match the target system. The build container is not a portable runtime bundle;
do not copy its glibc or x86-64 host libraries to the device. Keep the user's
`armada-game-launch` wrapper for Steam game-mode testing. No fixed CPU affinity
is applied.

## Verified so far

- Cross-compiled the public runtime, settings launcher and SDL3 to AArch64 ELF.
- 35/35 non-presenter runtime tests passed under QEMU (including fibers,
  scheduling, portable SSE/MMX semantics, virtual gamepad and dummy audio).
- Python suite: 50 tests, 2 skipped in the cross container, no failures.
- Native x86-64 regression: 36/36 runtime tests passed, including Vulkan presenter.
- Relocated the cross build mount and reran scheduling/audio tests successfully.
- SDL build enables X11/Wayland, Vulkan and ALSA/PipeWire/PulseAudio backends.
- ARM64 software-Vulkan presenter exceeded CTest's 60-second timeout. An
  extended 300-second run also did not complete, logged `AA present` with the
  CLOSED error during termination, and was stopped. Its cause is not established;
  do not count this as a pass or as proof of target-GPU incompatibility.
  Physical display/audio/input and the game are untested.

## Acceptance still required

- Build the real runner from the authenticated disc and inspect ELF dependencies.
- Confirm target Vulkan capabilities including BC1/2/3 and depth/stencil formats.
- Verify menus, demonstration and real fights, movies, saves and soft reset.
- Verify physical display, audio and controller in gamescope, including hotplug.
- Establish 480p, MSAA off, SMAA off performance before higher quality settings.
- Treat ARM64 reciprocal/reciprocal-square-root differences as a regression risk.

A public test passing is not evidence that a game mode or target GPU works.
