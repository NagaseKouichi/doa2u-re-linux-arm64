/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dsound_effects_image.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

enum { IMAGE_HEADER = 0x800u, IMAGE_PAYLOAD = 0x818u, SCRATCH_BASE = 0xc000u };

static uint32_t read_u32(const uint8_t *data)
{
    return (uint32_t)data[0] | (uint32_t)data[1] << 8 |
        (uint32_t)data[2] << 16 | (uint32_t)data[3] << 24;
}

static uint64_t stream_step(uint64_t state)
{
    uint64_t feedback = (state >> 63 ^ state ^ state >> 2 ^ state >> 3) & 1u;
    return state >> 1 | feedback << 63;
}

static void decode(uint64_t warmup, const uint8_t key[8],
    const uint8_t *encoded, uint8_t *decoded, uint32_t bytes)
{
    uint64_t stream = stream_step(warmup);
    for (uint32_t i = 0u; i < bytes; ++i) {
        uint32_t j = i & 7u;
        uint8_t fixed = (uint8_t)(warmup >> (j * 8u));
        stream = stream_step(stream);
        decoded[i] = (uint8_t)(encoded[i] - key[j] * fixed) ^ fixed ^ (uint8_t)stream;
    }
}

static bool range(uint32_t offset, uint32_t bytes, uint32_t start, uint32_t end)
{
    return offset >= start && offset <= end && bytes <= end - offset;
}

static bool overlap(uint32_t a, uint32_t a_bytes, uint32_t b, uint32_t b_bytes)
{
    return a_bytes != 0u && b_bytes != 0u &&
        (uint64_t)a < (uint64_t)b + b_bytes &&
        (uint64_t)b < (uint64_t)a + a_bytes;
}

void recomp_dsound_effects_destroy(RecompDsoundEffectsImage *image)
{
    if (image == NULL) return;
    free(image->payload);
    free(image->maps);
    *image = (RecompDsoundEffectsImage){0};
}

uint32_t recomp_dsound_effects_parse(const uint8_t *data, uint32_t bytes,
    uint64_t initializer, RecompDsoundEffectsImage *image)
{
    RecompDsoundEffectsImage next = {0};
    uint8_t *keys = NULL;
    uint8_t fixed_key[8];
    uint64_t state_start, descriptor;
    uint32_t code_end = 0u, state_end = 0u, result = RECOMP_DSOUND_INVALID_PARAM;
    if (data == NULL || image == NULL || initializer == 0u || bytes < IMAGE_PAYLOAD)
        return result;
    state_start = IMAGE_PAYLOAD + (uint64_t)read_u32(data + IMAGE_HEADER + 4u) * 4u;
    descriptor = state_start + (uint64_t)read_u32(data + IMAGE_HEADER + 12u) * 4u;
    if (descriptor > bytes || bytes - descriptor < 8u ||
        read_u32(data + IMAGE_HEADER + 8u) != state_start) return result;
    next.count = read_u32(data + (uint32_t)descriptor);
    if (next.count == 0u || next.count > RECOMP_DSOUND_MAX_EFFECTS ||
        next.count > (bytes - descriptor - 8u) / 40u)
        return result;
    next.payload_bytes = (uint32_t)descriptor - IMAGE_PAYLOAD;
    next.state_offset = (uint32_t)state_start - IMAGE_PAYLOAD;
    next.state_bytes = next.payload_bytes - next.state_offset;
    next.scratch_bytes = read_u32(data + (uint32_t)descriptor + 4u);
    if (next.state_offset == 0u ||
        (uint64_t)SCRATCH_BASE + next.scratch_bytes > UINT32_MAX) return result;
    next.maps = calloc(next.count, sizeof *next.maps);
    next.payload = malloc(next.payload_bytes);
    keys = malloc((size_t)next.count * 8u);
    if (next.maps == NULL || next.payload == NULL || keys == NULL) {
        result = RECOMP_DSOUND_OUT_OF_MEMORY;
        goto done;
    }
    memcpy(next.payload, data + IMAGE_PAYLOAD, next.payload_bytes);
    for (uint32_t i = 0u; i < next.count; ++i) {
        const uint8_t *wire = data + (uint32_t)descriptor + 8u + i * 32u;
        RecompDsoundEffectsMap *map = &next.maps[i];
        uint32_t code = read_u32(wire), state = read_u32(wire + 8u);
        uint32_t scratch = read_u32(wire + 24u);
        map->code_bytes = read_u32(wire + 4u);
        map->state_bytes = read_u32(wire + 12u);
        map->y_offset = read_u32(wire + 16u);
        map->y_bytes = read_u32(wire + 20u);
        map->scratch_bytes = read_u32(wire + 28u);
        if (!range(code, map->code_bytes, IMAGE_PAYLOAD, (uint32_t)state_start) ||
            map->code_bytes == 0u || ((code | map->code_bytes | state | map->state_bytes |
            scratch | map->scratch_bytes) & 3u) != 0u ||
            code - IMAGE_PAYLOAD != code_end ||
            !range(state, map->state_bytes, (uint32_t)state_start, (uint32_t)descriptor) ||
            state - IMAGE_PAYLOAD < state_end ||
            !range(scratch, map->scratch_bytes, SCRATCH_BASE,
                (uint32_t)((uint64_t)SCRATCH_BASE + next.scratch_bytes))) goto done;
        /* shortcut: Y storage is unused here; enable it after verifying a nonempty image. */
        if (map->y_offset != 0u || map->y_bytes != 0u) goto done;
        map->code_offset = code - IMAGE_PAYLOAD;
        map->state_offset = state - IMAGE_PAYLOAD;
        map->scratch_offset = scratch - SCRATCH_BASE;
        code_end = map->code_offset + map->code_bytes;
        state_end = map->state_offset + map->state_bytes;
        for (uint32_t j = 0u; j < i; ++j) {
            const RecompDsoundEffectsMap *prior = &next.maps[j];
            if (overlap(map->scratch_offset, map->scratch_bytes,
                prior->scratch_offset, prior->scratch_bytes)) goto done;
        }
    }
    for (uint32_t i = 0u; i < 81u; ++i) initializer = stream_step(initializer);
    for (uint32_t i = 0u; i < 8u; ++i) fixed_key[i] = (uint8_t)(initializer >> (i * 8u));
    decode(initializer, fixed_key, data + (uint32_t)descriptor + 8u + next.count * 32u,
        keys, next.count * 8u);
    for (uint32_t i = 0u; i < next.count; ++i) {
        RecompDsoundEffectsMap *map = &next.maps[i];
        decode(initializer, keys + i * 8u, next.payload + map->code_offset,
            next.payload + map->code_offset, map->code_bytes);
    }
    recomp_dsound_effects_destroy(image);
    *image = next;
    next = (RecompDsoundEffectsImage){0};
    result = RECOMP_DSOUND_OK;
done:
    free(keys);
    recomp_dsound_effects_destroy(&next);
    return result;
}
