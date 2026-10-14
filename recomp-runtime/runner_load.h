/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DOA2U_RUNNER_LOAD_H
#define DOA2U_RUNNER_LOAD_H

#include "xbe_parser.h"

bool recomp_load_xbe(const doaxbv::XbeParseResult& parsed,
    std::vector<std::uint8_t>& memory, std::string& error);

#endif
