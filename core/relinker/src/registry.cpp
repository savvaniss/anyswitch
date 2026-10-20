// SPDX-License-Identifier: MIT
#include "nx/relinker/registry.hpp"

#include "nxabi/nxabi.h"

#include "nx/common/file.hpp"
#include "nx/format/detect.hpp"
#include "nx/format/elf_reader.hpp"

#include <sstream>

namespace nx {

void Registry::add_file(const std::filesystem::path& path) {
    Bytes data = read_file(path);
    if (detect_file_kind(data) == FileKind::Elf) {
        ElfFile f = read_elf(data);
        const std::string prefix = NXABI_SYMBOL_PREFIX;
        for (const auto& s : f.dynamic_symbols) {
            if (s.shndx != elf::SHN_UNDEF && s.name.starts_with(prefix)) {
                names_.insert(s.name.substr(prefix.size()));
            }
        }
        return;
    }
    std::istringstream in(std::string(data.begin(), data.end()));
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }
        names_.insert(line);
    }
}

} // namespace nx
