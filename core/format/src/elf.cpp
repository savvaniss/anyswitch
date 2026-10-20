// SPDX-License-Identifier: MIT
#include "nx/format/elf.hpp"

namespace nx::elf {

const char* reloc_name(std::uint32_t type) {
    switch (type) {
    case R_AARCH64_NONE: return "R_AARCH64_NONE";
    case R_AARCH64_ABS64: return "R_AARCH64_ABS64";
    case R_AARCH64_ABS32: return "R_AARCH64_ABS32";
    case R_AARCH64_COPY: return "R_AARCH64_COPY";
    case R_AARCH64_GLOB_DAT: return "R_AARCH64_GLOB_DAT";
    case R_AARCH64_JUMP_SLOT: return "R_AARCH64_JUMP_SLOT";
    case R_AARCH64_RELATIVE: return "R_AARCH64_RELATIVE";
    case R_AARCH64_TLS_DTPMOD64: return "R_AARCH64_TLS_DTPMOD64";
    case R_AARCH64_TLS_DTPREL64: return "R_AARCH64_TLS_DTPREL64";
    case R_AARCH64_TLS_TPREL64: return "R_AARCH64_TLS_TPREL64";
    case R_AARCH64_TLSDESC: return "R_AARCH64_TLSDESC";
    case R_AARCH64_IRELATIVE: return "R_AARCH64_IRELATIVE";
    default: return "R_AARCH64_<unknown>";
    }
}

std::uint32_t sysv_hash(const char* name) {
    std::uint32_t h = 0;
    for (const auto* p = reinterpret_cast<const unsigned char*>(name); *p; ++p) {
        h = (h << 4) + *p;
        std::uint32_t g = h & 0xf0000000u;
        if (g) {
            h ^= g >> 24;
        }
        h &= ~g;
    }
    return h;
}

} // namespace nx::elf
