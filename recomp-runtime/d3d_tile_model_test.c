#include "d3d_tile_model.h"

#include <stdio.h>
#include <string.h>

static int expect(const char *name, bool condition)
{
    if (!condition) {
        fprintf(stderr, "D3D tile: %s failed\n", name);
        return 0;
    }
    return 1;
}

int recomp_d3d_tile_model_test(void)
{
    RecompD3dTile source = {{1u, 0x1000u, 3u, 4u, 5u, 6u}};
    RecompD3dTile destination;
    const RecompD3dTile zero = {{0}};
    uint32_t address = 0xdeadbeefu;
    int passed = 1;

    recomp_d3d_set_tile(&destination, &source);
    passed &= expect("ordinary tile clips depth fields",
        destination.words[0] == 1u && destination.words[1] == 0x1000u &&
        destination.words[2] == 3u && destination.words[3] == 4u &&
        destination.words[4] == 0u && destination.words[5] == 0u);
    source.words[0] |= 0x80000000u;
    recomp_d3d_set_tile(&destination, &source);
    passed &= expect("depth tile preserves fields",
        memcmp(&destination, &source, sizeof source) == 0);
    recomp_d3d_set_tile(&source, &source);
    passed &= expect("source and destination may alias",
        source.words[4] == 5u && source.words[5] == 6u);
    source.words[1] = 0u;
    recomp_d3d_set_tile(&destination, &source);
    passed &= expect("empty tile becomes zero",
        memcmp(&destination, &zero, sizeof zero) == 0);
    destination.words[0] = 0x80000000u;
    recomp_d3d_set_tile(&destination, NULL);
    passed &= expect("null tile becomes zero",
        memcmp(&destination, &zero, sizeof zero) == 0);
    passed &= expect("last tile uses verified array layout",
        recomp_d3d_tile_entry_address(0x10000u, 7u, &address) &&
        address == 0x10000u + 0x1ac0u + 7u * 0x18u);
    address = 0xdeadbeefu;
    passed &= expect("out of bounds index preserves address",
        !recomp_d3d_tile_entry_address(0x10000u, 8u, &address) &&
        address == 0xdeadbeefu);
    passed &= expect("wrapping index rejected",
        !recomp_d3d_tile_entry_address(0x10000u, UINT32_MAX, &address));
    passed &= expect("null device rejected",
        !recomp_d3d_tile_entry_address(0u, 0u, &address));
    passed &= expect("null output rejected",
        !recomp_d3d_tile_entry_address(0x10000u, 0u, NULL));
    passed &= expect("highest fitting device accepted",
        recomp_d3d_tile_entry_address(
            UINT32_MAX - RECOMP_D3D_TILE_ARRAY_EXTENT, 7u, &address));
    address = 0xdeadbeefu;
    passed &= expect("device overflow preserves address",
        !recomp_d3d_tile_entry_address(
            UINT32_MAX - RECOMP_D3D_TILE_ARRAY_EXTENT + 1u, 0u, &address) &&
        address == 0xdeadbeefu);
    passed &= expect("highest fitting tile source accepted",
        recomp_d3d_tile_range_fits(UINT32_MAX - RECOMP_D3D_TILE_ENTRY_SPAN));
    passed &= expect("overflowing tile source rejected",
        !recomp_d3d_tile_range_fits(UINT32_MAX - RECOMP_D3D_TILE_ENTRY_SPAN + 1u));
    passed &= expect("null tile source remains normalizable",
        recomp_d3d_tile_range_fits(0u));
    return passed;
}
