// SPDX-License-Identifier: MIT
// One in-memory representation for NSO and NRO executables.
//
// Sources: https://switchbrew.org/wiki/NSO, /wiki/NRO, /wiki/MOD, /wiki/Rtld,
// and libnx crt0 (ISC), which shows that the word at .text+4 is the offset of
// the MOD0 header from the module start and that MOD0 fields are relative to
// the MOD0 header itself.
#pragma once

#include "nx/common/bytes.hpp"
#include "nx/format/elf.hpp"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace nx {

enum class ModuleFormat { Nso, Nro };

struct Segment {
    std::uint64_t offset = 0; // offset within the module image
    std::uint64_t size = 0;
    std::uint64_t end() const { return offset + size; }
    bool contains(std::uint64_t at) const { return at >= offset && at < offset + size; }
};

struct Mod0 {
    std::uint64_t offset = 0; // image offset of the "MOD0" magic
    std::uint64_t dynamic = 0;
    std::uint64_t bss_start = 0;
    std::uint64_t bss_end = 0;
    std::uint64_t eh_frame_hdr_start = 0;
    std::uint64_t eh_frame_hdr_end = 0;
    std::uint64_t module_object = 0;
};

struct Symbol {
    std::string name;
    std::uint8_t info = 0;
    std::uint8_t other = 0;
    std::uint16_t shndx = 0;
    std::uint64_t value = 0;
    std::uint64_t size = 0;

    std::uint8_t bind() const { return elf::st_bind(info); }
    std::uint8_t type() const { return elf::st_type(info); }
    bool defined() const { return shndx != elf::SHN_UNDEF; }
    bool absolute() const { return shndx == elf::SHN_ABS; }
};

enum class RelocSource { Rela, Rel, Relr, Plt };

struct Reloc {
    std::uint64_t offset = 0; // image offset of the relocated word
    std::uint32_t type = 0;
    std::uint32_t sym = 0;
    std::int64_t addend = 0;
    RelocSource source = RelocSource::Rela;
};

struct Module {
    ModuleFormat format = ModuleFormat::Nso;
    std::string name;
    std::array<std::uint8_t, 32> module_id{};
    std::uint32_t nso_flags = 0;

    // The loaded image: text, ro and data at their memory offsets, bss zero
    // filled, padded to a page boundary.
    Bytes image;
    Segment text, ro, data, bss;

    Mod0 mod0;
    std::vector<elf::Dyn> dynamic;
    std::vector<Symbol> symbols; // index 0 is the null symbol
    std::vector<Reloc> relocs;

    std::uint64_t init = 0, fini = 0;
    std::uint64_t init_array = 0, init_array_size = 0;
    std::uint64_t fini_array = 0, fini_array_size = 0;
    std::uint64_t pltgot = 0;
    bool has_hash = false, has_gnu_hash = false;

    std::optional<std::uint64_t> dynamic_value(std::int64_t tag) const;
    const Symbol* find_export(const std::string& name) const;
    std::size_t import_count() const;
    std::size_t export_count() const;
};

// Parses an NSO or NRO file. verify_hashes checks NSO segment SHA-256 values
// when the header asks for it.
Module load_module(ByteSpan file, const std::string& name, bool verify_hashes = true);

} // namespace nx
