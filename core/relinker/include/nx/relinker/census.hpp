// SPDX-License-Identifier: MIT
// Phase 0 measurement: which instructions and relocations in a module need
// the runtime's attention. Answers PLAN.md open questions Q1 and Q7.
#pragma once

#include "nx/format/module.hpp"

#include <map>
#include <string>

namespace nx {

struct Census {
    std::string module;
    std::uint64_t text_words = 0;
    std::map<std::uint32_t, std::uint64_t> svc_by_number;
    std::map<std::string, std::uint64_t> sysreg_reads;  // by register name
    std::map<std::string, std::uint64_t> sysreg_writes; // by register name
    std::map<std::string, std::uint64_t> cache_ops;     // dc/ic by name
    std::map<std::string, std::uint64_t> exceptions;    // brk/hvc/smc/hlt
    std::uint64_t exclusive_unordered = 0;
    std::uint64_t exclusive_ordered = 0;
    std::map<std::string, std::uint64_t> relocations; // by type name
    std::uint64_t imports = 0;
    std::uint64_t exports = 0;
};

Census run_census(const Module& module);
std::string census_to_json(const Census& census);
std::string census_to_text(const Census& census);

} // namespace nx
