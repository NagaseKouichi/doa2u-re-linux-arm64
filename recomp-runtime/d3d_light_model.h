/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DOA2U_RECOMP_D3D_LIGHT_MODEL_H
#define DOA2U_RECOMP_D3D_LIGHT_MODEL_H

#include <stdbool.h>
#include <stdint.h>

typedef struct RecompD3dLightDesc {
    uint32_t type;
    float diffuse[4];
    float specular[4];
    float ambient[4];
    float position[3];
    float direction[3];
    float range;
    float falloff;
    float attenuation[3];
    float theta;
    float phi;
} RecompD3dLightDesc;

typedef struct RecompD3dLightRecord {
    RecompD3dLightDesc desc;
    uint32_t flags;
    float normalized_direction[3];
    float spot[5];
    uint32_t slot;
} RecompD3dLightRecord;

void recomp_d3d_light_default(RecompD3dLightDesc *desc);
bool recomp_d3d_light_set(
    RecompD3dLightRecord *record, const RecompD3dLightDesc *desc);
uint32_t recomp_d3d_light_slot_type(uint32_t type);

#endif
