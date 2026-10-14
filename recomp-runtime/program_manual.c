/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "program_manual.h"

RecompFunction recomp_lookup_manual(uint32_t guest_address)
{
    /* Enable each seam only after verifying its DOA2U address and ABI. */
    (void)guest_address;
    return NULL;
}
