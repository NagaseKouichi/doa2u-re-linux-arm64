# SPDX-License-Identifier: GPL-3.0-or-later
"""Configure a real cross compiler when available; no game files required."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(shutil.which('aarch64-linux-gnu-gcc') and shutil.which('cmake'),
                     'requires the Linux ARM64 cross-build environment')
class Arm64ToolchainTests(unittest.TestCase):
    def test_compiler_and_fiber_architecture_are_aarch64(self):
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / 'source'
            source.mkdir()
            (source / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.20)
project(arm64_check C ASM)
if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64" OR NOT CMAKE_CROSSCOMPILING)
  message(FATAL_ERROR "Not an AArch64 cross build")
endif()
add_library(check OBJECT check.c)
add_library(dep SHARED dep.c)
set_target_properties(dep PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/deps")
add_executable(probe main.c)
target_link_libraries(probe PRIVATE dep)
''')
            (source / 'dep.c').write_text('int value(void) { return 0; }')
            (source / 'main.c').write_text('int value(void); int main(void) { return value(); }')
            (source / 'check.c').write_text('''#ifndef __aarch64__
#error Wrong target compiler
#endif
_Static_assert(sizeof(void*) == 8, "64-bit pointers required");
int target_architecture(void) { return 64; }
''')
            build = Path(temp) / 'build'
            subprocess.run(['cmake', '-G', 'Ninja', '-S', str(source), '-B', str(build),
                            '-DCMAKE_TOOLCHAIN_FILE=' + str(ROOT / 'cmake/linux-aarch64.cmake')],
                           check=True, capture_output=True, text=True)
            subprocess.run(['cmake', '--build', str(build)], check=True,
                           capture_output=True, text=True)
            objects = list(build.rglob('check.c.o'))
            self.assertEqual(len(objects), 1)
            # ELF e_machine, AArch64 = 183; inspect bytes, not the filename.
            self.assertEqual(objects[0].read_bytes()[18:20], bytes((183, 0)))
            dynamic = subprocess.check_output(
                ['aarch64-linux-gnu-readelf', '-d', str(build / 'probe')], text=True)
            self.assertIn('$ORIGIN/deps', dynamic)
            self.assertNotIn(str(build), dynamic)


if __name__ == '__main__':
    unittest.main()
