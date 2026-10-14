/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dsound_device_model.h"
#include <stddef.h>

void recomp_dsound_device_reset(RecompDsoundDeviceModel *model)
{
    if (model != NULL) *model = (RecompDsoundDeviceModel){0};
}

uint32_t recomp_dsound_device_add_ref(RecompDsoundDeviceModel *model, uint32_t *count)
{
    if (model == NULL || count == NULL || model->handle == 0u || model->owners == 0u ||
        model->owners >= UINT32_MAX - RECOMP_DSOUND_RESERVED_REFS) {
        return RECOMP_DSOUND_INVALID_PARAM;
    }
    *count = ++model->owners + RECOMP_DSOUND_RESERVED_REFS;
    return RECOMP_DSOUND_OK;
}

uint32_t recomp_dsound_device_create(RecompDsoundDeviceModel *model,
    uint32_t handle, bool host_ready)
{
    uint32_t count;
    if (model == NULL) return RECOMP_DSOUND_INVALID_PARAM;
    if (!host_ready) return RECOMP_DSOUND_FAIL;
    if (model->handle != 0u) return recomp_dsound_device_add_ref(model, &count);
    if (handle == 0u) return RECOMP_DSOUND_OUT_OF_MEMORY;
    model->handle = handle;
    model->owners = 1u;
    return RECOMP_DSOUND_OK;
}

uint32_t recomp_dsound_device_release(RecompDsoundDeviceModel *model, uint32_t *count)
{
    if (model == NULL || count == NULL || model->handle == 0u || model->owners == 0u) {
        return RECOMP_DSOUND_INVALID_PARAM;
    }
    --model->owners;
    *count = model->owners == 0u ? 0u : model->owners + RECOMP_DSOUND_RESERVED_REFS;
    if (model->owners == 0u) model->handle = 0u;
    return RECOMP_DSOUND_OK;
}
