# SPDX-License-Identifier: GPL-3.0-or-later
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
# Tooling exists inside this build container, never on the handheld's rootfs.
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates cmake ninja-build gcc-aarch64-linux-gnu \
    g++-aarch64-linux-gnu qemu-user python3 git pkg-config \
    && rm -rf /var/lib/apt/lists/*
# Keep host repository and ARM64 ports distinct; no host-library fallback.
RUN dpkg --add-architecture arm64 && python3 -c "from pathlib import Path; p=Path('/etc/apt/sources.list.d/ubuntu.sources'); p.write_text(p.read_text().replace('Signed-By:', 'Architectures: amd64\nSigned-By:')); Path('/etc/apt/sources.list.d/arm64.sources').write_text('Types: deb\\nURIs: http://ports.ubuntu.com/ubuntu-ports\\nSuites: noble noble-updates noble-security\\nComponents: main universe\\nArchitectures: arm64\\nSigned-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg\\n')" \
    && apt-get update && apt-get install -y --no-install-recommends \
    libx11-dev:arm64 libxext-dev:arm64 libxrandr-dev:arm64 libxcursor-dev:arm64 \
    libxi-dev:arm64 libxss-dev:arm64 libxkbcommon-dev:arm64 libwayland-dev:arm64 \
    libegl-dev:arm64 libasound2-dev:arm64 libpulse-dev:arm64 \
    libpipewire-0.3-dev:arm64 libudev-dev:arm64 libdbus-1-dev:arm64 \
    libshaderc1:arm64 libvulkan1:arm64 mesa-vulkan-drivers:arm64 \
    libwayland-bin wayland-protocols \
    && rm -rf /var/lib/apt/lists/*
ENV PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig
RUN apt-get update && apt-get install -y --no-install-recommends python3-venv libxtst-dev:arm64 \
    && python3 -m venv /opt/doa2u-venv \
    && /opt/doa2u-venv/bin/pip install --no-cache-dir capstone==5.0.7 pillow==12.2.0 \
    && rm -rf /var/lib/apt/lists/*
ENV PATH=/opt/doa2u-venv/bin:$PATH
WORKDIR /work
