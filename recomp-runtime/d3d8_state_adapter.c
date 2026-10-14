/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "d3d8_state_adapter.h"
#include "d3d8_adapter.h"
#include "d3d_light_model.h"
#include "kernel_abi.h"
#include "stop_report.h"

#include <string.h>

enum {
    D3D_DIRTY = 0x00355058u,
    D3D_FOG_COLOR = 0x00355488u,
    D3D_OUT_OF_MEMORY = 0x8007000eu,
};

static void set_gamma(void)
{
    uint32_t device = recomp_d3d8_require_device();
    uint32_t flags = kernel_arg(1u), ramp = kernel_arg(2u);
    uint32_t bank = *recomp_memory_u32(device + 0x240cu);
    RecompD3dPresenterCommand command = {0};
    if (bank > 1u) recomp_stop(2, "d3d8:invalid-gamma-bank");
    command.type = RECOMP_D3D_PRESENTER_COMMAND_GAMMA;
    recomp_guest_load(command.data.gamma, ramp, sizeof command.data.gamma);
    if (memcmp(command.data.gamma, recomp_memory(device + 0x1e04u + bank * 0x300u,
            sizeof command.data.gamma),
            sizeof command.data.gamma) != 0) {
        bank = *recomp_memory_u32(device + 0x2478u) & 1u;
        recomp_guest_store(device + 0x1e04u + bank * 0x300u,
            command.data.gamma, sizeof command.data.gamma);
        *recomp_memory_u32(device + 0x240cu) = bank;
        if ((flags & 2u) == 0u) *recomp_memory_u32(device + 0x2404u + bank * 4u) = 1u;
        recomp_d3d8_submit(&command);
    }
    recomp_runtime.registers.esp += 12u;
}

static void set_transform(void)
{
    static const uint32_t dirty[10] = {
        0xff0200u, 0x200u, 0x400u, 0x400u, 0x400u,
        0x400u, 0x200u, 0x200u, 0x200u, 0x200u,
    };
    uint32_t device = recomp_d3d8_require_device();
    uint32_t state = kernel_arg(1u), matrix = kernel_arg(2u);
    float m[16];
    if (state >= 10u) recomp_stop(2, "d3d8:unsupported-transform:%u", state);
    recomp_guest_load(m, matrix, sizeof m);
    recomp_guest_store(device + 0xc60u + state * 0x40u, m, sizeof m);
    if (state == 1u) {
        float near_z = 0.0f, far_z = 1.0f, inverse_far = 1.0f;
        uint32_t m44;
        if (m[10] != 0.0f && m[10] != m[11]) {
            near_z = (float)(m[15] - ((double)m[14] / m[10]) * m[11]);
            far_z = (float)(m[15] + (((double)m[15] - m[14]) /
                ((double)m[10] - m[11])) * m[11]);
            inverse_far = 1.0f / far_z;
        }
        recomp_guest_store(device + 0x93cu, &near_z, sizeof near_z);
        recomp_guest_store(device + 0x940u, &far_z, sizeof far_z);
        recomp_guest_store(device + 0x944u, &inverse_far, sizeof inverse_far);
        memcpy(&m44, &m[15], sizeof m44);
        uint32_t *mode = recomp_memory_u32(device + 8u);
        *mode = (*mode & ~2u) | (m[3] == 0.0f && m[7] == 0.0f &&
            m[11] == 0.0f && m44 == 0x3f800000u ? 2u : 0u);
    }
    *recomp_memory_u32(D3D_DIRTY) |= dirty[state];
    recomp_runtime.registers.esp += 12u;
}

static void set_fog_color(void)
{
    recomp_d3d8_require_device();
    *recomp_memory_u32(D3D_FOG_COLOR) = kernel_arg(1u);
    recomp_runtime.registers.esp += 8u;
}

static uint32_t light_record(uint32_t device, uint32_t index)
{
    uint32_t array = *recomp_memory_u32(device + 0x7a4u);
    uint32_t capacity = *recomp_memory_u32(device + 0x7a8u);
    if (capacity > RECOMP_XBOX_RAM_SIZE / sizeof(RecompD3dLightRecord) ||
        (capacity != 0u && array == 0u)) {
        recomp_stop(2, "d3d8:invalid-light-array");
    }
    if (capacity != 0u) recomp_memory(array, capacity * sizeof(RecompD3dLightRecord));
    if (index >= capacity) {
        uint64_t grown = ((uint64_t)index + 16u) & ~UINT64_C(15);
        uint64_t bytes = grown * sizeof(RecompD3dLightRecord);
        if (bytes > RECOMP_XBOX_RAM_SIZE) return 0u;
        uint32_t replacement = recomp_kernel_allocate_pool((uint32_t)bytes);
        if (replacement == 0u) return 0u;
        if (capacity != 0u) {
            recomp_guest_memcpy(replacement, array, capacity * sizeof(RecompD3dLightRecord));
        }
        for (uint32_t i = capacity; i < grown; ++i) {
            RecompD3dLightRecord empty = {0};
            empty.slot = UINT32_MAX;
            recomp_guest_store(replacement + i * sizeof empty, &empty, sizeof empty);
        }
        for (uint32_t slot = 0u; slot < 8u; ++slot) {
            uint32_t *active = recomp_memory_u32(device + 0x7acu + slot * 4u);
            if (*active != 0u) {
                uint32_t offset = *active - array;
                if (offset >= capacity * sizeof(RecompD3dLightRecord) ||
                    offset % sizeof(RecompD3dLightRecord) != 0u) {
                    recomp_stop(2, "d3d8:invalid-light-slot");
                }
                *active = replacement + offset;
            }
        }
        *recomp_memory_u32(device + 0x7a4u) = replacement;
        *recomp_memory_u32(device + 0x7a8u) = (uint32_t)grown;
        if (array != 0u) recomp_kernel_free_pool(array);
        array = replacement;
    }
    return array + index * sizeof(RecompD3dLightRecord);
}

static void mark_light(uint32_t device, uint32_t address,
    const RecompD3dLightRecord *record)
{
    if (record->slot == UINT32_MAX) return;
    uint32_t slot = record->slot;
    if (slot >= 8u || *recomp_memory_u32(device + 0x7acu + slot * 4u) != address) {
        recomp_stop(2, "d3d8:invalid-light-slot");
    }
    uint32_t *types = recomp_memory_u32(device + 0x7ccu);
    *types = (*types & ~(3u << (slot * 2u))) |
        (recomp_d3d_light_slot_type(record->desc.type) << (slot * 2u));
    *recomp_memory_u32(D3D_DIRTY) |= 0x10000u << slot;
}

static uint32_t store_light(uint32_t device, uint32_t index,
    const RecompD3dLightDesc *desc)
{
    /* shortcut: spot coefficients need this SDK's approximation tables; stop until observed. */
    if (desc->type == 2u) recomp_stop(2, "d3d8:unsupported-spot-light");
    if (recomp_d3d_light_slot_type(desc->type) == 0u) {
        recomp_stop(2, "d3d8:unsupported-light-type:%u", desc->type);
    }
    uint32_t address = light_record(device, index);
    if (address == 0u) return D3D_OUT_OF_MEMORY;
    RecompD3dLightRecord record;
    recomp_guest_load(&record, address, sizeof record);
    if (!recomp_d3d_light_set(&record, desc)) recomp_stop(2, "d3d8:invalid-light-direction");
    recomp_guest_store(address, &record, sizeof record);
    mark_light(device, address, &record);
    return 0u;
}

static void set_light(void)
{
    uint32_t device = recomp_d3d8_require_device();
    RecompD3dLightDesc desc;
    recomp_guest_load(&desc, kernel_arg(2u), sizeof desc);
    kernel_return(2u, store_light(device, kernel_arg(1u), &desc));
}

static void light_enable(void)
{
    uint32_t device = recomp_d3d8_require_device();
    uint32_t index = kernel_arg(1u), enable = kernel_arg(2u);
    uint32_t address = light_record(device, index);
    if (address == 0u) { kernel_return(2u, D3D_OUT_OF_MEMORY); return; }
    RecompD3dLightRecord record;
    recomp_guest_load(&record, address, sizeof record);
    if ((record.flags & 1u) == 0u) {
        RecompD3dLightDesc desc;
        recomp_d3d_light_default(&desc);
        uint32_t result = store_light(device, index, &desc);
        if (result != 0u) { kernel_return(2u, result); return; }
        recomp_guest_load(&record, address, sizeof record);
    }
    if (recomp_d3d_light_slot_type(record.desc.type) == 0u) {
        recomp_stop(2, "d3d8:unsupported-light-type:%u", record.desc.type);
    }
    record.flags |= 4u;
    if (enable != 0u && record.slot == UINT32_MAX) {
        uint32_t types = *recomp_memory_u32(device + 0x7ccu), slot;
        for (slot = 0u; slot < 8u && ((types >> (slot * 2u)) & 3u) != 0u; ++slot) {}
        if (slot == 8u) recomp_stop(2, "d3d8:unsupported-active-light-count");
        record.slot = slot;
        *recomp_memory_u32(device + 0x7acu + slot * 4u) = address;
        mark_light(device, address, &record);
    } else if (enable == 0u && record.slot != UINT32_MAX) {
        uint32_t slot = record.slot;
        mark_light(device, address, &record);
        *recomp_memory_u32(device + 0x7acu + slot * 4u) = 0u;
        *recomp_memory_u32(device + 0x7ccu) &= ~(3u << (slot * 2u));
        record.slot = UINT32_MAX;
    }
    recomp_guest_store(address, &record, sizeof record);
    kernel_return(2u, 0u);
}

RecompFunction recomp_d3d8_state_lookup_manual(uint32_t address)
{
    /* Verified 5849 API entries; spot lights and >8 active lights stop explicitly. */
    switch (address) {
    case 0x00344180u: return set_gamma;
    case 0x00344210u: return set_transform;
    case 0x003465c0u: return set_fog_color;
    case 0x00345040u: return set_light;
    case 0x00345300u: return light_enable;
    default: return NULL;
    }
}
