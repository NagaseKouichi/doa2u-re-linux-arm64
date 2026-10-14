/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DOA2U_RECOMP_DSOUND_DEVICE_MODEL_H
#define DOA2U_RECOMP_DSOUND_DEVICE_MODEL_H

#include "dsound_buffer_model.h"
#include <stdbool.h>

typedef struct RecompDsoundDeviceModel {
    uint32_t handle;
    uint32_t owners;
    uint32_t hrtf_mode;
} RecompDsoundDeviceModel;

/* The native host owns the five reserved SDK references without APU voices. */
enum { RECOMP_DSOUND_RESERVED_REFS = 5u, RECOMP_DSOUND_FAIL = 0x80004005u };
void recomp_dsound_device_reset(RecompDsoundDeviceModel *model);
uint32_t recomp_dsound_device_create(RecompDsoundDeviceModel *model,
    uint32_t handle, bool host_ready);
uint32_t recomp_dsound_device_add_ref(RecompDsoundDeviceModel *model, uint32_t *count);
uint32_t recomp_dsound_device_release(RecompDsoundDeviceModel *model, uint32_t *count);

#endif
