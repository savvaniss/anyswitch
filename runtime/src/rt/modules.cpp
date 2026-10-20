// SPDX-License-Identifier: MIT
// Finds every relinked module in the process through its NXABI_PT_MODULE_INFO
// program header.
#include "nxrt/runtime.hpp"

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <link.h>

namespace nxrt {
namespace {

std::vector<LoadedModule> g_modules;

int collect(struct dl_phdr_info* info, size_t, void*) {
    for (int i = 0; i < info->dlpi_phnum; ++i) {
        const ElfW(Phdr)& ph = info->dlpi_phdr[i];
        if (ph.p_type != NXABI_PT_MODULE_INFO) {
            continue;
        }
        auto* mi = reinterpret_cast<const nxabi_module_info*>(info->dlpi_addr + ph.p_vaddr);
        if (mi->magic != NXABI_MODULE_INFO_MAGIC) {
            fatal(1, "module %s: bad module info magic", info->dlpi_name);
        }
        if (mi->abi_version != NXABI_VERSION) {
            fatal(1, "module '%.32s' was relinked for nxabi v%u but this runtime implements v%u; relink it",
                  mi->name, mi->abi_version, NXABI_VERSION);
        }
        LoadedModule m;
        m.info = mi;
        m.base = info->dlpi_addr;
        m.name.assign(mi->name, strnlen(mi->name, sizeof mi->name));
        g_modules.push_back(std::move(m));
    }
    return 0;
}

} // namespace

void discover_modules() {
    g_modules.clear();
    dl_iterate_phdr(collect, nullptr);
    std::sort(g_modules.begin(), g_modules.end(), [](const LoadedModule& a, const LoadedModule& b) {
        return a.info->load_order < b.info->load_order;
    });
    for (const auto& m : g_modules) {
        log("module %-8s order %u base %#" PRIxPTR " image %#" PRIxPTR " patches %u traps %u", m.name.c_str(),
            m.info->load_order, m.base, m.at(m.info->image_offset), m.info->patch_count, m.info->trap_count);
    }
}

const std::vector<LoadedModule>& modules() { return g_modules; }

const LoadedModule* main_module() {
    for (const auto& m : g_modules) {
        if (m.info->flags & NXABI_MODULE_MAIN) {
            return &m;
        }
    }
    return nullptr;
}

const LoadedModule* module_for_address(std::uintptr_t address) {
    for (const auto& m : g_modules) {
        if (m.contains_image(address)) {
            return &m;
        }
    }
    return nullptr;
}

const LoadedModule* module_for_info(const nxabi_module_info* info) {
    for (const auto& m : g_modules) {
        if (m.info == info) {
            return &m;
        }
    }
    return nullptr;
}

std::string describe_address(std::uintptr_t address) {
    char buf[256];
    if (const LoadedModule* m = module_for_address(address)) {
        std::snprintf(buf, sizeof buf, "%s+%#" PRIxPTR, m->name.c_str(), address - m->at(m->info->image_offset));
        return buf;
    }
    Dl_info dl{};
    if (dladdr(reinterpret_cast<void*>(address), &dl) && dl.dli_fname) {
        const char* file = dl.dli_fname;
        if (const char* slash = std::strrchr(file, '/')) {
            file = slash + 1;
        }
        if (dl.dli_sname) {
            std::snprintf(buf, sizeof buf, "%s!%s+%#" PRIxPTR, file, demangle(dl.dli_sname).c_str(),
                          address - reinterpret_cast<std::uintptr_t>(dl.dli_saddr));
        } else {
            std::snprintf(buf, sizeof buf, "%s+%#" PRIxPTR, file,
                          address - reinterpret_cast<std::uintptr_t>(dl.dli_fbase));
        }
        return buf;
    }
    std::snprintf(buf, sizeof buf, "%#" PRIxPTR, address);
    return buf;
}

} // namespace nxrt
