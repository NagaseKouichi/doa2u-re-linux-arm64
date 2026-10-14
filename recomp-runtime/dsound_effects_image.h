/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DOA2U_RECOMP_DSOUND_EFFECTS_IMAGE_H
#define DOA2U_RECOMP_DSOUND_EFFECTS_IMAGE_H

#include "dsound_buffer_model.h"

/* Native download budget, independent of the SDK's image format. */
enum { RECOMP_DSOUND_MAX_EFFECTS = 256u };

typedef struct RecompDsoundEffectsMap {
    uint32_t code_offset, code_bytes;
    uint32_t state_offset, state_bytes;
    uint32_t y_offset, y_bytes;
    uint32_t scratch_offset, scratch_bytes;
} RecompDsoundEffectsMap;

typedef struct RecompDsoundEffectsImage {
    uint8_t *payload;
    uint32_t payload_bytes;
    RecompDsoundEffectsMap *maps;
    uint32_t count;
    uint32_t state_offset, state_bytes;
    uint32_t scratch_bytes, y_bytes;
} RecompDsoundEffectsImage;

/* Initialize the model to zero. Success replaces it; failure leaves it intact.
   Code/state offsets address owned payload; scratch/Y offsets address separate
   native regions. The caller supplies the selected library's initializer. */
uint32_t recomp_dsound_effects_parse(const uint8_t *data, uint32_t bytes,
    uint64_t initializer, RecompDsoundEffectsImage *image);
void recomp_dsound_effects_destroy(RecompDsoundEffectsImage *image);

#endif
