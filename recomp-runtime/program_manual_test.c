/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "program_manual.h"

int main(void)
{
    return recomp_lookup_manual(0u) != NULL ||
        recomp_lookup_manual(1u) != NULL ||
        recomp_lookup_manual(UINT32_MAX) != NULL;
}
