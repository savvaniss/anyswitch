// SPDX-License-Identifier: MIT
// Reached when guest code calls an import the runtime does not provide. The
// relinker bound the call to a stub carrying the import's index.
#include "nxrt/runtime.hpp"

#include <cstdio>
#include <cstring>

extern "C" [[noreturn]] void nxrt_trap(const nxabi_module_info* info, std::uint32_t index) {
    using namespace nxrt;
    const LoadedModule* m = module_for_info(info);
    const char* name = "?";
    if (m && index < info->trap_count) {
        auto* table = reinterpret_cast<const std::uint8_t*>(m->at(info->trap_names_offset));
        std::uint32_t offset;
        std::memcpy(&offset, table + index * 4, sizeof offset);
        name = reinterpret_cast<const char*>(table + offset);
    }
    std::string pretty = demangle(name);
    fatal(kExitUnimplemented, "unimplemented guest function %s%s%s%s (imported by %s)", pretty.c_str(),
          pretty == name ? "" : " [", pretty == name ? "" : name, pretty == name ? "" : "]",
          m ? m->name.c_str() : "?");
}
