/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dsound_device_model.h"
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL device model line %d\n", __LINE__); return 1; } } while (0)
int main(void)
{
    RecompDsoundDeviceModel model = {0}, prior;
    uint32_t count;
    CHECK(recomp_dsound_device_create(&model, 0x1008u, false) == RECOMP_DSOUND_FAIL);
    CHECK(model.handle == 0u && model.owners == 0u);
    CHECK(recomp_dsound_device_create(&model, 0u, true) == RECOMP_DSOUND_OUT_OF_MEMORY);
    CHECK(recomp_dsound_device_create(&model, 0x1008u, true) == RECOMP_DSOUND_OK);
    CHECK(model.handle == 0x1008u && model.owners == 1u);
    model.hrtf_mode = 3u;
    CHECK(recomp_dsound_device_create(&model, 0x2008u, true) == RECOMP_DSOUND_OK);
    CHECK(model.handle == 0x1008u && model.owners == 2u && model.hrtf_mode == 3u);
    CHECK(recomp_dsound_device_add_ref(&model, &count) == RECOMP_DSOUND_OK && count == 8u);
    CHECK(recomp_dsound_device_release(&model, &count) == RECOMP_DSOUND_OK && count == 7u);
    CHECK(recomp_dsound_device_release(&model, &count) == RECOMP_DSOUND_OK && count == 6u);
    CHECK(recomp_dsound_device_release(&model, &count) == RECOMP_DSOUND_OK && count == 0u);
    CHECK(model.handle == 0u && model.owners == 0u);
    CHECK(recomp_dsound_device_release(&model, &count) == RECOMP_DSOUND_INVALID_PARAM);
    CHECK(recomp_dsound_device_create(&model, 0x2008u, true) == RECOMP_DSOUND_OK);
    model.owners = UINT32_MAX - RECOMP_DSOUND_RESERVED_REFS;
    prior = model;
    CHECK(recomp_dsound_device_add_ref(&model, &count) == RECOMP_DSOUND_INVALID_PARAM);
    CHECK(memcmp(&model, &prior, sizeof model) == 0);
    CHECK(recomp_dsound_device_create(&model, model.handle, false) == RECOMP_DSOUND_FAIL);
    CHECK(memcmp(&model, &prior, sizeof model) == 0);
    recomp_dsound_device_reset(&model);
    CHECK(model.handle == 0u && model.owners == 0u && model.hrtf_mode == 0u);
    return 0;
}
