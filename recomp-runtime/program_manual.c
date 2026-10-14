/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "program_manual.h"
#include "stop_report.h"
#include <stdio.h>

static void host_kernel_init_segment(void)
{
    /* XAPI 5849's void/ret-0 kernel compatibility entry patches old kernels
       and tail-calls INIT/LDT setup. The native host owns kernel lifetime. */
    fprintf(stderr, "recomp xapi: kernel INIT segment owned by host\n");
    recomp_runtime.registers.esp += 4u;
}

static void unbound_create_device(void)
{
    /* D3D8 5849: stop at the six-argument API until the semantic host device
       exists; generated CDevice::Init reaches NV2A registers on this path. */
    recomp_stop(2, "d3d8:create-device-unbound");
}

RecompFunction recomp_lookup_manual(uint32_t guest_address)
{
    if (guest_address == 0x002bd531u) return host_kernel_init_segment;
    if (guest_address == 0x0034a470u) return unbound_create_device;
    return NULL;
}
