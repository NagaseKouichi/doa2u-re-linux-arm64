/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "d3d_light_model.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL light model line %d: %s\n", __LINE__, #condition); return 1; \
} } while (0)

int main(void)
{
    RecompD3dLightDesc desc;
    RecompD3dLightRecord record = {0}, before;
    recomp_d3d_light_default(&desc);
    CHECK(desc.type == 3u && desc.diffuse[0] == 1.0f && desc.diffuse[2] == 1.0f);
    CHECK(desc.diffuse[3] == 0.0f && desc.direction[2] == 1.0f && desc.range == 0.0f);
    record.flags = 4u;
    record.slot = 2u;
    desc.direction[0] = 3.0f;
    desc.direction[1] = 4.0f;
    desc.direction[2] = 0.0f;
    CHECK(recomp_d3d_light_set(&record, &desc));
    CHECK(record.flags == 7u && record.slot == 2u);
    CHECK(fabsf(record.normalized_direction[0] + 0.6f) < 1e-6f);
    CHECK(fabsf(record.normalized_direction[1] + 0.8f) < 1e-6f);
    CHECK(recomp_d3d_light_slot_type(3u) == 1u && recomp_d3d_light_slot_type(1u) == 2u);
    desc.type = 1u;
    desc.direction[0] = NAN;
    CHECK(recomp_d3d_light_set(&record, &desc));
    CHECK(record.desc.type == 1u && record.normalized_direction[0] == -0.6f);
    before = record;
    desc.type = 2u;
    CHECK(!recomp_d3d_light_set(&record, &desc));
    CHECK(memcmp(&record, &before, sizeof record) == 0);
    desc.type = 3u;
    desc.direction[0] = desc.direction[1] = desc.direction[2] = 0.0f;
    CHECK(!recomp_d3d_light_set(&record, &desc));
    CHECK(memcmp(&record, &before, sizeof record) == 0);
    desc.direction[0] = INFINITY;
    CHECK(!recomp_d3d_light_set(&record, &desc));
    CHECK(!recomp_d3d_light_set(NULL, &desc) && !recomp_d3d_light_set(&record, NULL));
    puts("PASS light defaults, normalization, point state and rejected inputs");
    return 0;
}
