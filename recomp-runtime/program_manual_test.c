/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "program_manual.h"
#include <setjmp.h>
#include <string.h>

RecompRuntime recomp_runtime;
static jmp_buf stopped;
static int stop_code;
static const char *stop_reason;

void recomp_stop(int code, const char *reason, ...)
{
    stop_code = code;
    stop_reason = reason;
    longjmp(stopped, 1);
}

int main(void)
{
    RecompFunction init_segment = recomp_lookup_manual(0x002bd531u);
    if (init_segment == NULL) return 1;
    recomp_runtime.registers.esp = 0x1000u;
    recomp_runtime.registers.eax = 0x12345678u;
    init_segment();
    RecompFunction create_device = recomp_lookup_manual(0x0034a470u);
    if (create_device == NULL) return 1;
    if (setjmp(stopped) == 0) {
        create_device();
        return 1;
    }
    if (stop_code != 2 || strcmp(stop_reason, "d3d8:create-device-unbound") != 0) return 1;
    return recomp_runtime.registers.esp != 0x1004u ||
        recomp_runtime.registers.eax != 0x12345678u ||
        recomp_lookup_manual(0u) != NULL ||
        recomp_lookup_manual(1u) != NULL ||
        recomp_lookup_manual(UINT32_MAX) != NULL;
}
