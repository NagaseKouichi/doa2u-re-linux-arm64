/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "device_model.h"
#include "kernel_abi.h"
#include "xbox_memory_layout.h"

#include <stdio.h>

enum {
    TEST_MEMORY_BASE = 0x10000u,
    TEST_MEMORY_SIZE = 0x1000u,
    TEST_ENTRY_ESP = TEST_MEMORY_BASE + 0x800u,
    TEST_OUTPUT = TEST_MEMORY_BASE + 0x900u,
    TEST_DRIVER = TEST_MEMORY_BASE + 0xa00u,
};

static int expect_u32(const char *name, uint32_t actual, uint32_t expected)
{
    if (actual == expected) return 1;
    fprintf(stderr, "kernel device: %s was 0x%08x, expected 0x%08x\n",
        name, actual, expected);
    return 0;
}

static uint32_t allocation_size(uint32_t address)
{
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    *recomp_memory_u32(TEST_ENTRY_ESP) = 0u;
    *recomp_memory_u32(TEST_ENTRY_ESP + 4u) = address;
    recomp_kernel_memory(180u)();
    return recomp_runtime.registers.eax;
}

int recomp_kernel_device_test(void)
{
    static uint8_t memory[TEST_MEMORY_SIZE];
    const RecompMemoryRegion region = {TEST_MEMORY_BASE, sizeof memory, memory};
    recomp_runtime_init(&region, 1u, NULL, 0u, NULL, 0u);
    if (!xbox_HeapSetImageEnd(TEST_MEMORY_BASE)) return 0;

    RecompDeviceCreateResult result = recomp_device_create(
        TEST_DRIVER, 0x170u, 0x3au, TEST_OUTPUT);
    int passed = expect_u32("model status", result.status, 0u);
    if (result.status != 0u || result.device_object == 0u ||
        result.device_extension == 0u) return 0;
    passed &= expect_u32("output pointer", *recomp_memory_u32(TEST_OUTPUT), result.device_object);
    passed &= expect_u32("object allocation", allocation_size(result.device_object), 0x48u);
    passed &= expect_u32("extension allocation", allocation_size(result.device_extension), 0x170u);
    passed &= expect_u32("object and extension do not overlap",
        (uint64_t)result.device_object + 0x48u <= result.device_extension ||
        (uint64_t)result.device_extension + 0x170u <= result.device_object, 1u);
    passed &= expect_u32("extension pointer",
        *recomp_memory_u32(result.device_object + 0x18u), result.device_extension);
    recomp_guest_memset(result.device_extension, 0xa5, 0x170u);
    *recomp_memory_u32(result.device_object + 0x40u) = 0x11223344u;
    *recomp_memory_u32(result.device_object + 0x44u) = 0x55667788u;
    passed &= expect_u32("object tail preserves extension",
        *recomp_memory_u32(result.device_extension), 0xa5a5a5a5u);
    passed &= expect_u32("object tail preserves second extension word",
        *recomp_memory_u32(result.device_extension + 4u), 0xa5a5a5a5u);
    recomp_guest_memset(result.device_extension, 0, 0x170u);
    passed &= expect_u32("extension clear preserves object tail",
        *recomp_memory_u32(result.device_object + 0x44u), 0x55667788u);
    passed &= expect_u32("driver object", *recomp_memory_u32(result.device_object + 8u), TEST_DRIVER);
    passed &= expect_u32("device type",
        (uint8_t)*recomp_memory_i8(result.device_object + 0x1cu), 0x3au);

    const uint32_t arguments[] = {TEST_DRIVER + 4u, 0u, 0u, 7u, 0u, TEST_OUTPUT + 4u};
    for (unsigned i = 0u; i < 6u; ++i) {
        *recomp_memory_u32(TEST_ENTRY_ESP + 4u + i * 4u) = arguments[i];
    }
    recomp_runtime.registers.esp = TEST_ENTRY_ESP;
    recomp_kernel_device(65u)();
    passed &= expect_u32("bridge status", recomp_runtime.registers.eax, 0u);
    passed &= expect_u32("bridge stack cleanup", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 28u);
    uint32_t object = *recomp_memory_u32(TEST_OUTPUT + 4u);
    if (object == 0u) return 0;
    passed &= expect_u32("bridge driver object", *recomp_memory_u32(object + 8u), TEST_DRIVER + 4u);
    passed &= expect_u32("bridge device type", (uint8_t)*recomp_memory_i8(object + 0x1cu), 7u);
    passed &= expect_u32("empty extension", *recomp_memory_u32(object + 0x18u), 0u);
    recomp_kernel_free_pool(result.device_extension);
    recomp_kernel_free_pool(result.device_object);
    recomp_kernel_free_pool(object);

    RecompFunction phy_init = recomp_kernel_device(253u);
    RecompFunction phy_link = recomp_kernel_device(252u);
    passed &= expect_u32("PHY initialize lookup", phy_init != NULL, 1u);
    passed &= expect_u32("PHY link lookup", phy_link != NULL, 1u);
    if (phy_init == NULL || phy_link == NULL) return 0;
    *recomp_memory_u32(TEST_OUTPUT) = 0xabcdef01u;
    for (uint32_t mode = 0u; mode < 2u; ++mode) {
        *recomp_memory_u32(TEST_ENTRY_ESP) = 0x12345678u;
        *recomp_memory_u32(TEST_ENTRY_ESP + 4u) = mode;
        *recomp_memory_u32(TEST_ENTRY_ESP + 8u) = TEST_OUTPUT;
        recomp_runtime.registers.esp = TEST_ENTRY_ESP;
        phy_init();
        passed &= expect_u32("PHY initialization unavailable", recomp_runtime.registers.eax, 0x801f0001u);
        passed &= expect_u32("PHY initialize stack", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 12u);
        passed &= expect_u32("PHY optional parameter preserved", *recomp_memory_u32(TEST_OUTPUT), 0xabcdef01u);
        recomp_runtime.registers.esp = TEST_ENTRY_ESP;
        phy_link();
        passed &= expect_u32("PHY cached/update link disconnected", recomp_runtime.registers.eax, 0u);
        passed &= expect_u32("PHY link stack", recomp_runtime.registers.esp, TEST_ENTRY_ESP + 8u);
        passed &= expect_u32("PHY return slot", *recomp_memory_u32(TEST_ENTRY_ESP), 0x12345678u);
    }
    return passed;
}
