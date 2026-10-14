/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "runner_load.h"
#include "runtime.h"
#include "xbox_memory_layout.h"

#include <cstring>
#include <iostream>

namespace {
std::uint32_t readU32(const std::vector<std::uint8_t>& memory, std::uint32_t address)
{
    std::uint32_t value;
    std::memcpy(&value, memory.data() + address, sizeof value);
    return value;
}

bool check(bool condition, const char *name)
{
    if (!condition) std::cerr << "runner load: " << name << " failed\n";
    return condition;
}
}

int main()
{
    doaxbv::XbeParseResult fixture;
    auto& metadata = fixture.metadata;
    metadata.header.baseAddress = 0x10000u;
    metadata.header.sizeImage = 0x2000u;
    metadata.header.sizeHeaders = 0x200u;
    metadata.header.stackCommit = 0x10000u;
    metadata.selectedEntryPoint = 0x11000u;
    metadata.selectedKernelThunkAddress = 0x10100u;
    metadata.kernelThunks = {{0u, 0x80000144u, 324u}, {1u, 0x800000ffu, 255u},
        {2u, 0x80000162u, 354u}, {3u, 0x80000164u, 356u}};
    metadata.tls = doaxbv::XbeTls{0x11000u, 0x11004u, 0x10180u, 0u, 168u, 0u};
    doaxbv::XbeSection section;
    section.virtualAddress = 0x11000u;
    section.virtualSize = 0x100u;
    section.rawAddress = 0x200u;
    section.rawSize = 4u;
    section.digestMatches = true;
    metadata.sections = {section};
    fixture.fileBytes.resize(0x204u, 0x5au);
    std::vector<std::uint8_t> memory(RECOMP_XBOX_RAM_SIZE, 0u);
    std::string error;
    bool passed = check(recomp_load_xbe(fixture, memory, error), "load synthetic image");
    passed &= check(readU32(memory, 0x11000u) == 0x5a5a5a5au, "section bytes");
    passed &= check(memory[0x11004u] == 0u, "section zero fill");
    passed &= check(readU32(memory, 0x10100u) == XBOX_KERNEL_DATA_BASE + 0x10u, "data thunk");
    passed &= check(readU32(memory, 0x10104u) == 0x800000ffu, "callable ordinal thunk");
    passed &= check(readU32(memory, 0x10108u) + 0x100u <= readU32(memory, 0x1010cu), "keys do not overlap boot mode");
    passed &= check(readU32(memory, XBOX_PCR_BASE + 0x28u) == XBOX_STARTUP_THREAD_OBJECT, "startup PCR");
    passed &= check(readU32(memory, XBOX_STARTUP_THREAD_STACK_SLOT + 4u) == 0x5a5a5a5au, "initialized TLS");
    passed &= check(readU32(memory, XBOX_PCR_BASE + 4u) == XBOX_STARTUP_THREAD_STACK_SLOT + 180u, "TLS bound");
    passed &= check(memory[XBOX_STARTUP_THREAD_STACK_SLOT + 8u] == 0u, "TLS zero fill");
    passed &= check(readU32(memory, readU32(memory, XBOX_PCR_BASE + 4u) - 180u) ==
        XBOX_STARTUP_THREAD_STACK_SLOT + 4u, "negative TLS index resolves pointer slot");
    passed &= check(readU32(memory, 0x10180u) == 0x5a5a5a5au, "TLS index belongs to XAPI startup");
    passed &= check(readU32(memory, XBOX_PCR_BASE + 0x1cu) == XBOX_PCR_BASE, "PCR self pointer");
    passed &= check(readU32(memory, XBOX_PCR_BASE + 0x20u) == XBOX_PCR_BASE + 0x28u, "PRCB pointer");
    std::memset(memory.data(), 0xcc, 0x1000u);
    const std::uint32_t physicalMarker = 0x12345678u;
    std::memcpy(memory.data() + 0x278u, &physicalMarker, sizeof physicalMarker);
    passed &= check(readU32(memory, XBOX_PCR_BASE) == 0xffffffffu, "physical page zero preserves exception chain");
    passed &= check(readU32(memory, XBOX_PCR_BASE + 0x28u) == XBOX_STARTUP_THREAD_OBJECT, "physical page zero preserves thread");
    passed &= check(readU32(memory, XBOX_PCR_BASE + 4u) == XBOX_STARTUP_THREAD_STACK_SLOT + 180u, "physical page zero preserves TLS bound");
    passed &= check(readU32(memory, XBOX_STARTUP_THREAD_STACK_SLOT + 4u) == 0x5a5a5a5au, "physical page zero preserves TLS data");
    const auto prcb = readU32(memory, XBOX_PCR_BASE + 0x20u);
    passed &= check(readU32(memory, prcb + 0x250u) == 0u, "physical page zero preserves PRCB debug callback");
    auto invalid = fixture;
    invalid.metadata.header.sizeImage = XBOX_GUEST_ALLOC_LIMIT;
    passed &= check(!recomp_load_xbe(invalid, memory, error), "image overlaps reservation");
    invalid = fixture;
    invalid.metadata.sections[0].virtualAddress = 0xfffffff0u;
    passed &= check(!recomp_load_xbe(invalid, memory, error), "section address overflow");
    invalid = fixture;
    invalid.metadata.sections[0].rawSize = 0x200u;
    passed &= check(!recomp_load_xbe(invalid, memory, error), "section source overflow");
    invalid = fixture;
    invalid.metadata.sections[0].digestMatches = false;
    passed &= check(!recomp_load_xbe(invalid, memory, error), "section authentication");
    invalid = fixture;
    invalid.metadata.header.stackCommit = RECOMP_XBOX_RAM_SIZE;
    passed &= check(!recomp_load_xbe(invalid, memory, error), "stack overflow");
    invalid = fixture;
    invalid.metadata.tls->sizeOfZeroFill = RECOMP_XBOX_RAM_SIZE;
    passed &= check(!recomp_load_xbe(invalid, memory, error), "TLS overflow");
    invalid = fixture;
    invalid.metadata.tls->dataEndAddress = 0x10000u;
    passed &= check(!recomp_load_xbe(invalid, memory, error), "reversed TLS range");
    invalid = fixture;
    invalid.metadata.tls->tlsIndexAddress = 0xfffffffcu;
    passed &= check(!recomp_load_xbe(invalid, memory, error), "TLS index overflow");
    invalid = fixture;
    invalid.metadata.selectedKernelThunkAddress = 0xfffffffcu;
    passed &= check(!recomp_load_xbe(invalid, memory, error), "thunk overflow");
    return passed ? 0 : 1;
}
