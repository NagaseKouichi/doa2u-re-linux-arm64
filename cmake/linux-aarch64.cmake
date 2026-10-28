# SPDX-License-Identifier: GPL-3.0-or-later
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
# A copied build tree must not retain the container's /work path for SDL3.
set(CMAKE_BUILD_RPATH_USE_ORIGIN TRUE)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_ASM_COMPILER aarch64-linux-gnu-gcc)
# Debian/Ubuntu cross compiler provides its own libc search paths. Additional
# target libraries (SDL3 etc.) can be installed in this isolated prefix.
set(RECOMP_TARGET_PREFIX "" CACHE PATH "Optional target-only dependency prefix")
# Debian multiarch -dev packages install shared headers in /usr/include
# and target libraries in /usr/lib/aarch64-linux-gnu, outside the compiler
# prefix. Permit that multiarch root while excluding x86-64 library prefixes.
set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu /)
set(CMAKE_IGNORE_PREFIX_PATH /usr/lib/x86_64-linux-gnu /lib/x86_64-linux-gnu)
set(ENV{PKG_CONFIG_LIBDIR} "/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig")
if(RECOMP_TARGET_PREFIX)
    list(PREPEND CMAKE_FIND_ROOT_PATH "${RECOMP_TARGET_PREFIX}")
endif()
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
# Opt in explicitly: cross compilation must not require QEMU or imply a GPU test.
if(DEFINED ENV{RECOMP_QEMU_AARCH64})
    set(CMAKE_CROSSCOMPILING_EMULATOR "$ENV{RECOMP_QEMU_AARCH64};-L;/usr/aarch64-linux-gnu" CACHE STRING "ARM64 test emulator")
endif()
