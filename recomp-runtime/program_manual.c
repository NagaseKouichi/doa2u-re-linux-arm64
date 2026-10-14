/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "program_manual.h"
#include "d3d8_adapter.h"
#include "d3d8_state_adapter.h"
#include "stop_report.h"
#include <stdio.h>

static void host_kernel_init_segment(void)
{
    /* XAPI 5849's void/ret-0 kernel compatibility entry patches old kernels
       and tail-calls INIT/LDT setup. The native host owns kernel lifetime. */
    fprintf(stderr, "recomp xapi: kernel INIT segment owned by host\n");
    recomp_runtime.registers.esp += 4u;
}

static void unbound_direct_sound_create(void)
{
    /* DSOUND 5849: three-argument API; its generated factory initializes APU
       interrupts and MMIO. Bind a native audio device before entering it. */
    recomp_stop(2, "dsound:create-device-unbound");
}

RecompFunction recomp_lookup_manual(uint32_t guest_address)
{
    if (guest_address == 0x002bd531u) return host_kernel_init_segment;
    if (guest_address == 0x00366210u) return unbound_direct_sound_create;
    RecompFunction function = recomp_d3d8_lookup_manual(guest_address);
    if (function == NULL) function = recomp_d3d8_state_lookup_manual(guest_address);
    return function;
}
