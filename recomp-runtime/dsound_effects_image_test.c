/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "dsound_effects_image.h"
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL effects image line %d\n", __LINE__); return 1; } } while (0)
enum { BASE = 0x818u, STATE = BASE + 68u, DESC = STATE + 96u, TOTAL = DESC + 88u };
static const uint64_t initializer = UINT64_C(0x0123456789abcdef);

static void write_u32(uint8_t *data, uint32_t value)
{
    for (unsigned i = 0u; i < 4u; ++i) data[i] = (uint8_t)(value >> (i * 8u));
}

/* Independent bit-array oracle, with no production stream implementation. */
static void step(uint8_t bits[64])
{
    uint8_t feedback = bits[63] ^ bits[0] ^ bits[2] ^ bits[3];
    memmove(bits, bits + 1u, 63u);
    bits[63] = feedback;
}

static uint8_t byte(const uint8_t bits[64], unsigned index)
{
    uint8_t value = 0u;
    for (unsigned i = 0u; i < 8u; ++i) value |= bits[index * 8u + i] << i;
    return value;
}

static void encode(const uint8_t *plain, uint8_t *encoded, uint32_t bytes,
    const uint8_t *key)
{
    uint8_t bits[64], fixed[8];
    for (unsigned i = 0u; i < 64u; ++i) bits[i] = (uint8_t)(initializer >> i & 1u);
    for (unsigned i = 0u; i < 81u; ++i) step(bits);
    for (unsigned i = 0u; i < 8u; ++i) fixed[i] = byte(bits, i);
    if (key == NULL) key = fixed;
    step(bits);
    for (uint32_t i = 0u; i < bytes; ++i) {
        unsigned j = i & 7u;
        step(bits);
        encoded[i] = (uint8_t)((plain[i] ^ fixed[j] ^ byte(bits, 0u)) + key[j] * fixed[j]);
    }
}

static void fixture(uint8_t data[TOTAL])
{
    uint8_t keys[16], plain[32];
    memset(data, 0u, TOTAL);
    write_u32(data + 0x804u, 17u);
    write_u32(data + 0x808u, STATE);
    write_u32(data + 0x80cu, 24u);
    write_u32(data + DESC, 2u);
    write_u32(data + DESC + 4u, 32u);
    for (unsigned i = 0u; i < 16u; ++i) keys[i] = (uint8_t)(0x01u + 0x22u * i);
    for (unsigned i = 0u; i < 2u; ++i) {
        uint8_t *map = data + DESC + 8u + i * 32u;
        write_u32(map, BASE + i * 32u);
        write_u32(map + 4u, 32u);
        write_u32(map + 8u, STATE + i * 48u);
        write_u32(map + 12u, 48u);
        write_u32(map + 24u, 0xc000u + i * 16u);
        write_u32(map + 28u, 16u);
        for (unsigned j = 0u; j < 32u; ++j) plain[j] = (uint8_t)(i * 32u + j);
        encode(plain, data + BASE + i * 32u, 32u, keys + i * 8u);
    }
    for (unsigned i = 0u; i < 96u; ++i) data[STATE + i] = (uint8_t)(0x80u + i);
    encode(keys, data + DESC + 72u, 16u, NULL);
}

static int rejects(const uint8_t *data, uint32_t bytes, RecompDsoundEffectsImage *image)
{
    RecompDsoundEffectsImage before = *image;
    uint8_t payload[164];
    memcpy(payload, image->payload, sizeof payload);
    CHECK(recomp_dsound_effects_parse(data, bytes, initializer, image) == RECOMP_DSOUND_INVALID_PARAM);
    CHECK(memcmp(&before, image, sizeof before) == 0);
    CHECK(memcmp(payload, image->payload, sizeof payload) == 0);
    return 0;
}

int main(void)
{
    uint8_t data[TOTAL], bad[TOTAL];
    RecompDsoundEffectsImage image = {0};
    static const uint8_t vector[32] = {
        0x0c, 0x70, 0x1a, 0xc2, 0x7a, 0x81, 0xf5, 0xaa,
        0x18, 0xe6, 0x61, 0x9a, 0x92, 0x91, 0x69, 0x14,
        0x71, 0xa3, 0x2c, 0x1b, 0xe3, 0xb6, 0xef, 0xc5,
        0x8d, 0x99, 0xab, 0xd7, 0xf9, 0x2d, 0xaf, 0xf1
    };
    fixture(data);
    CHECK(memcmp(data + BASE, vector, sizeof vector) == 0);
    CHECK(recomp_dsound_effects_parse(data, TOTAL, initializer, &image) == RECOMP_DSOUND_OK);
    CHECK(image.count == 2u && image.payload_bytes == 164u);
    CHECK(image.state_offset == 68u && image.state_bytes == 96u);
    CHECK(image.scratch_bytes == 32u && image.y_bytes == 0u);
    CHECK(image.maps[0].code_offset == 0u && image.maps[1].code_offset == 32u);
    CHECK(image.maps[0].state_offset == 68u && image.maps[1].state_offset == 116u);
    CHECK(image.maps[0].scratch_offset == 0u && image.maps[1].scratch_offset == 16u);
    for (unsigned i = 0u; i < 64u; ++i) CHECK(image.payload[i] == i);
    CHECK(memcmp(image.payload + 68u, data + STATE, 96u) == 0);
    CHECK(memcmp(data + BASE, vector, sizeof vector) == 0);
    for (uint32_t i = 0u; i < TOTAL; ++i) CHECK(rejects(data, i, &image) == 0);
    CHECK(rejects(NULL, TOTAL, &image) == 0);
    CHECK(recomp_dsound_effects_parse(data, TOTAL, 0u, &image) == RECOMP_DSOUND_INVALID_PARAM);
    CHECK(recomp_dsound_effects_parse(data, TOTAL, initializer, NULL) == RECOMP_DSOUND_INVALID_PARAM);

    const struct { uint32_t offset, value; } mutations[] = {
        {0x804u, UINT32_MAX}, {0x80cu, UINT32_MAX}, {0x808u, STATE + 4u},
        {DESC, 0u}, {DESC, UINT32_MAX}, {DESC, 3u}, {DESC + 4u, UINT32_MAX},
        {DESC + 8u, BASE + 4u}, {DESC + 12u, 0u}, {DESC + 12u, 72u},
        {DESC + 12u, 31u}, {DESC + 16u, BASE}, {DESC + 20u, 100u},
        {DESC + 24u, 4u}, {DESC + 28u, 4u}, {DESC + 32u, 0xbffcu},
        {DESC + 36u, 36u}, {DESC + 40u, BASE}, {DESC + 48u, STATE},
        {DESC + 64u, 0xc000u}, {DESC + 64u, 0xc020u},
        {DESC + 16u, 0xfe830000u}, {DESC + 8u, 0xfe838000u}
    };
    for (unsigned i = 0u; i < sizeof mutations / sizeof mutations[0]; ++i) {
        memcpy(bad, data, sizeof bad);
        write_u32(bad + mutations[i].offset, mutations[i].value);
        CHECK(rejects(bad, TOTAL, &image) == 0);
    }
    {
        enum { COUNT = RECOMP_DSOUND_MAX_EFFECTS + 1u,
            LARGE_STATE = BASE + COUNT * 4u, LARGE_DESC = LARGE_STATE + COUNT * 4u,
            LARGE_TOTAL = LARGE_DESC + 8u + COUNT * 40u };
        uint8_t oversized[LARGE_TOTAL] = {0};
        write_u32(oversized + 0x804u, COUNT);
        write_u32(oversized + 0x808u, LARGE_STATE);
        write_u32(oversized + 0x80cu, COUNT);
        write_u32(oversized + LARGE_DESC, COUNT);
        for (unsigned i = 0u; i < COUNT; ++i) {
            uint8_t *map = oversized + LARGE_DESC + 8u + i * 32u;
            write_u32(map, BASE + i * 4u);
            write_u32(map + 4u, 4u);
            write_u32(map + 8u, LARGE_STATE + i * 4u);
            write_u32(map + 12u, 4u);
            write_u32(map + 24u, 0xc000u);
        }
        CHECK(rejects(oversized, sizeof oversized, &image) == 0);
    }
    memcpy(bad, data, sizeof bad);
    write_u32(bad + DESC + 64u, 0xc000u);
    write_u32(bad + DESC + 68u, 0u);
    CHECK(recomp_dsound_effects_parse(bad, TOTAL, initializer, &image) == RECOMP_DSOUND_OK);
    memset(bad, 0u, sizeof bad);
    for (unsigned i = 0u; i < 64u; ++i) CHECK(image.payload[i] == i);
    recomp_dsound_effects_destroy(&image);
    CHECK(image.payload == NULL && image.maps == NULL && image.count == 0u);
    recomp_dsound_effects_destroy(&image);
    recomp_dsound_effects_destroy(NULL);
    return 0;
}
