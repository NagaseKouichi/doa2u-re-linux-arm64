/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "program_manual.h"
RecompRuntime recomp_runtime;

RecompFunction recomp_d3d8_lookup_manual(uint32_t address)
{
    (void)address;
    return NULL;
}

RecompFunction recomp_d3d8_state_lookup_manual(uint32_t address)
{
    (void)address;
    return NULL;
}

RecompFunction recomp_dsound_api_lookup_manual(uint32_t address)
{
    (void)address;
    return NULL;
}

int main(void)
{
    RecompFunction init_segment = recomp_lookup_manual(0x002bd531u);
    if (init_segment == NULL) return 1;
    recomp_runtime.registers.esp = 0x1000u;
    recomp_runtime.registers.eax = 0x12345678u;
    init_segment();
    return recomp_runtime.registers.esp != 0x1004u ||
        recomp_runtime.registers.eax != 0x12345678u ||
        recomp_lookup_manual(0u) != NULL ||
        recomp_lookup_manual(1u) != NULL ||
        recomp_lookup_manual(UINT32_MAX) != NULL;
}
