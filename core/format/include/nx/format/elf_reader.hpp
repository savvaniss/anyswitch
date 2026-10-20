// SPDX-License-Identifier: MIT
// Minimal ELF64 reader: enough to read program headers, sections and the
// dynamic symbol table of the runtime libraries and of relinker output.
#pragma once

#include "nx/common/bytes.hpp"
#include "nx/format/elf.hpp"

#include <string>
#include <vector>

namespace nx {

struct ElfSymbolView {
    std::string name;
    std::uint8_t info = 0;
    std::uint16_t shndx = 0;
    std::uint64_t value = 0;
};

struct ElfFile {
    elf::Ehdr header{};
    std::vector<elf::Phdr> segments;
    std::vector<elf::Shdr> sections;
    std::vector<std::string> section_names;
    std::vector<ElfSymbolView> dynamic_symbols;
    std::vector<elf::Dyn> dynamic;

    const elf::Phdr* find_segment(std::uint32_t type) const;
    const elf::Shdr* find_section(const std::string& name) const;
};

ElfFile read_elf(ByteSpan data);

} // namespace nx
