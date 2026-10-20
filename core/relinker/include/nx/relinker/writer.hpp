// SPDX-License-Identifier: MIT
// Converts one loaded module into a host ELF (AArch64 Linux): a PIE main
// executable or a shared object. Layout and contract: docs/dev/ARCHITECTURE.md.
#pragma once

#include "nx/format/module.hpp"
#include "nx/format/npdm.hpp"
#include "nx/relinker/patcher.hpp"
#include "nx/relinker/registry.hpp"

#include <map>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace nx {

enum class OutputKind { Main, Shared };

struct ConvertOptions {
    OutputKind kind = OutputKind::Main;
    std::string module_name = "main";
    std::uint32_t load_order = 0;
    std::vector<std::string> needed;
    std::string soname;
    std::string runpath = "$ORIGIN/lib";
    std::string interpreter = "/lib/ld-linux-aarch64.so.1";
    bool patch = true;
    PatchScanOptions scan;
    // When set, imports that neither the registry nor a sibling module
    // provides are bound to trap stubs that report the missing name.
    const Registry* registry = nullptr;
    std::unordered_set<std::string> sibling_exports;
    std::optional<Npdm> npdm;
    Bytes npdm_raw;
};

struct ConvertReport {
    std::size_t imports = 0;
    std::size_t exports = 0;
    std::size_t relocations = 0;
    std::map<std::string, std::size_t> patches; // by patch kind name
    std::vector<std::string> trapped;           // missing function imports bound to traps
    std::vector<std::string> unresolved_data;   // missing non-call imports left for the loader
    bool text_relocations = false;
};

struct ConvertResult {
    Bytes elf;
    ConvertReport report;
};

ConvertResult convert_module(const Module& module, const ConvertOptions& options);

} // namespace nx
