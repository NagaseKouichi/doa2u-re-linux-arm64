/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "recomp_types.h"
#include <stdio.h>

static unsigned manual_calls, generated_calls, lookups;
static bool bound;

static void manual_body(void) { ++manual_calls; }
static void generated_body(void) { ++generated_calls; }

RecompFunction recomp_lookup_manual(uint32_t address)
{
    ++lookups;
    return bound && address == 0x1000u ? manual_body : NULL;
}

int main(void)
{
    bound = true;
    RECOMP_ABI_CALL(0x1000u, generated_body);
    bound = false;
    RECOMP_ABI_CALL(0x1000u, generated_body);
    bound = true;
    RECOMP_ABI_CALL(0x1004u, generated_body);
    if (lookups != 3u || manual_calls != 1u || generated_calls != 2u) {
        fprintf(stderr, "ABI call: manual=%u generated=%u lookups=%u\n",
            manual_calls, generated_calls, lookups);
        return 1;
    }
    return 0;
}
