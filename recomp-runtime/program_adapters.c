/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "kernel_abi.h"
#include "program_manual.h"
#include "stop_report.h"
#include <inttypes.h>
#include <stdio.h>

extern RecompFunction recomp_lookup(uint32_t guest_address);

void recomp_program_missing(uint32_t guest_address)
{
    fprintf(
        stderr,
        "recomp program: missing generated body at 0x%08" PRIx32 "\n",
        guest_address);
    recomp_stop(2, "missing-body:0x%08" PRIx32, guest_address);
}

RecompFunction recomp_program_lookup(uint32_t guest_address)
{
    RecompFunction function = recomp_lookup_manual(guest_address);

    if (function == NULL) {
        function = recomp_lookup(guest_address);
    }
    if (function == NULL) {
        function = recomp_lookup_kernel(guest_address);
    }
    return function;
}
