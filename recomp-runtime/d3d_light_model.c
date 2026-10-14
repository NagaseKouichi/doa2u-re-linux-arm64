/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "d3d_light_model.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

_Static_assert(sizeof(RecompD3dLightDesc) == 0x68u, "D3DLIGHT8 size");
_Static_assert(sizeof(RecompD3dLightRecord) == 0x90u, "light record size");
_Static_assert(offsetof(RecompD3dLightRecord, slot) == 0x8cu, "light slot offset");

void recomp_d3d_light_default(RecompD3dLightDesc *desc)
{
    memset(desc, 0, sizeof *desc);
    desc->type = 3u;
    desc->diffuse[0] = desc->diffuse[1] = desc->diffuse[2] = 1.0f;
    desc->direction[2] = 1.0f;
}

uint32_t recomp_d3d_light_slot_type(uint32_t type)
{
    return type == 3u ? 1u : type == 1u ? 2u : 0u;
}

bool recomp_d3d_light_set(
    RecompD3dLightRecord *record, const RecompD3dLightDesc *desc)
{
    float direction[3];
    if (record == NULL || desc == NULL || recomp_d3d_light_slot_type(desc->type) == 0u) {
        return false;
    }
    if (desc->type == 3u) {
        double length = 0.0;
        for (uint32_t i = 0u; i < 3u; ++i) {
            length += (double)desc->direction[i] * desc->direction[i];
        }
        if (!isfinite(length) || length <= 0.0) return false;
        length = sqrt(length);
        for (uint32_t i = 0u; i < 3u; ++i) {
            direction[i] = (float)(-desc->direction[i] / length);
        }
    }
    record->desc = *desc;
    record->flags |= 3u;
    if (desc->type == 3u) memcpy(record->normalized_direction, direction, sizeof direction);
    return true;
}
