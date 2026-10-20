// SPDX-License-Identifier: MIT
// META and ACI0 layouts from https://switchbrew.org/wiki/NPDM
#include "nx/format/npdm.hpp"

namespace nx {

Npdm parse_npdm(ByteSpan file) {
    Reader r(file, "npdm");
    if (!r.matches(0, "META")) {
        fail("npdm: missing META magic");
    }
    r.require(0, 0x80);
    Npdm npdm;
    std::uint8_t flags0 = r.u8(0xC);
    npdm.is_64bit = (flags0 & 1) != 0;
    npdm.address_space = (flags0 >> 1) & 7;
    npdm.main_thread_priority = r.u8(0xE);
    npdm.main_thread_core = r.u8(0xF);
    npdm.system_resource_size = r.u32(0x14);
    npdm.version = r.u32(0x18);
    npdm.main_thread_stack_size = r.u32(0x1C);
    for (std::uint64_t i = 0; i < 0x10; ++i) {
        char c = static_cast<char>(r.u8(0x20 + i));
        if (c == '\0') {
            break;
        }
        npdm.name.push_back(c);
    }
    std::uint32_t aci_offset = r.u32(0x70);
    std::uint32_t aci_size = r.u32(0x74);
    if (aci_size >= 0x18 && r.matches(aci_offset, "ACI0")) {
        npdm.program_id = r.u64(aci_offset + 0x10);
    }
    return npdm;
}

const char* address_space_name(std::uint32_t value) {
    switch (value) {
    case 0: return "32-bit";
    case 1: return "64-bit (36-bit)";
    case 2: return "32-bit (no reserved)";
    case 3: return "64-bit (39-bit)";
    case 4: return "64-bit (42-bit)";
    default: return "unknown";
    }
}

} // namespace nx
