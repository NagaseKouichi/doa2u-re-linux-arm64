/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "host_diagnostics.h"
#include "runner_load.h"
#include "save_transaction.h"
#include "xbox_memory_layout.h"
extern "C" {
#include "kernel_abi.h"
#include "stop_report.h"
RecompFunction recomp_program_lookup(std::uint32_t address);
}

#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char **argv)
{
    recomp_install_host_diagnostics();
    std::string xbePath, discRoot, expectStop, milestoneLog;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if ((argument != "--xbe" && argument != "--disc-root" &&
             argument != "--expect-stop" && argument != "--milestone-log") || i + 1 >= argc) {
            std::cerr << "usage: recomp_program_runner --xbe <path> [--disc-root <path>]"
                " [--expect-stop <id>] [--milestone-log <path>]\n";
            return 64;
        }
        const std::string value = argv[++i];
        if (argument == "--xbe") xbePath = value;
        else if (argument == "--disc-root") discRoot = value;
        else if (argument == "--expect-stop") expectStop = value;
        else milestoneLog = value;
    }
    if (xbePath.empty()) {
        std::cerr << "recomp runner: --xbe is required\n";
        return 64;
    }
    recomp_stop_configure(expectStop.empty() ? nullptr : expectStop.c_str(),
        milestoneLog.empty() ? nullptr : milestoneLog.c_str());
    const auto parsed = doaxbv::XbeParser().parse(std::filesystem::u8path(xbePath));
    if (!parsed.ok()) {
        std::cerr << "recomp runner: " << parsed.errors.front() << '\n';
        return 1;
    }
    if (parsed.metadata.fileSha256 != RECOMP_XBE_SHA256) {
        std::cerr << "recomp runner: XBE SHA-256 differs from the authenticated program target\n";
        return 1;
    }
    std::vector<std::uint8_t> memory(RECOMP_XBOX_RAM_SIZE, 0u);
    std::string error;
    if (!recomp_load_xbe(parsed, memory, error)) {
        std::cerr << "recomp runner: " << error << '\n';
        return 1;
    }
    const auto imageEnd = static_cast<std::uint64_t>(parsed.metadata.header.baseAddress) +
        parsed.metadata.header.sizeImage;
    if (!xbox_HeapSetImageEnd(static_cast<std::uint32_t>(imageEnd))) {
        std::cerr << "recomp runner: invalid heap image boundary\n";
        return 1;
    }
    if (discRoot.empty()) discRoot = std::filesystem::absolute(std::filesystem::u8path(xbePath)).parent_path().string();
    const auto discPath = std::filesystem::u8path(discRoot);
    std::error_code filesystemError;
    if (!std::filesystem::is_directory(discPath, filesystemError)) {
        std::cerr << "recomp runner: disc root is not a readable directory\n";
        return 1;
    }
    recomp_disc_root_path = discRoot.c_str();
    if (!recomp_save_initialize(recomp_disc_root_path)) {
        std::cerr << "recomp runner: save recovery failed; guest not started\n";
        return 2;
    }
    const RecompMemoryRegion region = {0u, memory.size(), memory.data()};
    recomp_runtime_init(&region, 1u, nullptr, 0u, nullptr, 0u);
    recomp_runtime_set_lookup(recomp_program_lookup);
    recomp_runtime.registers.esp = XBOX_STARTUP_STACK_TOP;
    std::fprintf(stderr, "recomp runner: entry=0x%08x image_end=0x%08x kernel-only\n",
        parsed.metadata.selectedEntryPoint, static_cast<unsigned>(imageEnd));
    recomp_watchdog_start();
    recomp_runtime.registers.esp -= 4u;
    *recomp_memory_u32(recomp_runtime.registers.esp) = 0u;
    recomp_dispatch_indirect_site(parsed.metadata.selectedEntryPoint,
        XBOX_STARTUP_STACK_TOP, __FILE__, __LINE__);
    recomp_kernel_run_threads();
    recomp_stop(0, "program-returned");
}
