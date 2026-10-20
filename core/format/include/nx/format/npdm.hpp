// SPDX-License-Identifier: MIT
// NPDM (program metadata) reader. Layout from https://switchbrew.org/wiki/NPDM
#pragma once

#include "nx/common/bytes.hpp"

#include <optional>
#include <string>

namespace nx {

struct Npdm {
    bool is_64bit = true;
    std::uint32_t address_space = 0; // META Flags0 bits 1-3
    std::uint32_t main_thread_priority = 0;
    std::uint32_t main_thread_core = 0;
    std::uint32_t main_thread_stack_size = 0;
    std::uint32_t system_resource_size = 0;
    std::uint32_t version = 0;
    std::string name;
    std::optional<std::uint64_t> program_id; // from ACI0, when present
};

Npdm parse_npdm(ByteSpan file);
const char* address_space_name(std::uint32_t value);

} // namespace nx
