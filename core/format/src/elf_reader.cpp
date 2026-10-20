// SPDX-License-Identifier: MIT
#include "nx/format/elf_reader.hpp"

namespace nx {

const elf::Phdr* ElfFile::find_segment(std::uint32_t type) const {
    for (const auto& p : segments) {
        if (p.type == type) {
            return &p;
        }
    }
    return nullptr;
}

const elf::Shdr* ElfFile::find_section(const std::string& name) const {
    for (std::size_t i = 0; i < sections.size(); ++i) {
        if (section_names[i] == name) {
            return &sections[i];
        }
    }
    return nullptr;
}

ElfFile read_elf(ByteSpan data) {
    Reader r(data, "elf");
    if (!r.matches(0, "\x7f" "ELF") || r.u8(4) != 2 || r.u8(5) != 1) {
        fail("not a little-endian ELF64 file");
    }
    ElfFile f;
    f.header = r.read<elf::Ehdr>(0);
    for (std::uint16_t i = 0; i < f.header.phnum; ++i) {
        f.segments.push_back(r.read<elf::Phdr>(f.header.phoff + std::uint64_t(i) * sizeof(elf::Phdr)));
    }
    for (std::uint16_t i = 0; i < f.header.shnum; ++i) {
        f.sections.push_back(r.read<elf::Shdr>(f.header.shoff + std::uint64_t(i) * sizeof(elf::Shdr)));
    }
    if (!f.sections.empty() && f.header.shstrndx < f.sections.size()) {
        const auto& names = f.sections[f.header.shstrndx];
        Reader strtab(r.slice(names.offset, names.size), "shstrtab");
        for (const auto& s : f.sections) {
            f.section_names.push_back(strtab.cstr(s.name));
        }
    } else {
        f.section_names.assign(f.sections.size(), std::string());
    }
    for (const auto& s : f.sections) {
        if (s.type != elf::SHT_DYNSYM) {
            continue;
        }
        const auto& str = f.sections.at(s.link);
        Reader strtab(r.slice(str.offset, str.size), "dynstr");
        for (std::uint64_t at = s.offset; at + sizeof(elf::Sym) <= s.offset + s.size; at += sizeof(elf::Sym)) {
            auto sym = r.read<elf::Sym>(at);
            f.dynamic_symbols.push_back({sym.name ? strtab.cstr(sym.name) : std::string(), sym.info, sym.shndx,
                                         sym.value});
        }
    }
    if (const auto* dyn = f.find_segment(elf::PT_DYNAMIC)) {
        for (std::uint64_t at = dyn->offset; at + sizeof(elf::Dyn) <= dyn->offset + dyn->filesz;
             at += sizeof(elf::Dyn)) {
            auto d = r.read<elf::Dyn>(at);
            if (d.tag == elf::DT_NULL) {
                break;
            }
            f.dynamic.push_back(d);
        }
    }
    return f;
}

} // namespace nx
