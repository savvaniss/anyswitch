// SPDX-License-Identifier: MIT
// NSO/NRO loading. Header layouts: https://switchbrew.org/wiki/NSO and
// https://switchbrew.org/wiki/NRO. MOD0: https://switchbrew.org/wiki/MOD.
// Relocation handling mirrors what https://switchbrew.org/wiki/Rtld documents.
#include "nx/format/module.hpp"

#include "nx/common/sha256.hpp"
#include "nx/format/detect.hpp"
#include "nx/format/lz4.hpp"

#include <algorithm>

namespace nx {
namespace {

constexpr std::uint64_t kPage = 0x1000;

struct NsoSegmentHeader {
    std::uint32_t file_offset;
    std::uint32_t mem_offset;
    std::uint32_t size;
    std::uint32_t file_size;
};

void place(Bytes& image, std::uint64_t at, ByteSpan data) {
    if (at + data.size() > image.size()) {
        image.resize(at + data.size());
    }
    std::copy(data.begin(), data.end(), image.begin() + static_cast<std::ptrdiff_t>(at));
}

void check_segment_order(const Module& m) {
    if (m.text.offset != 0) {
        fail("{}: .text must start at offset 0 (found {:#x})", m.name, m.text.offset);
    }
    if (m.text.end() > m.ro.offset || m.ro.end() > m.data.offset) {
        fail("{}: segments overlap or are out of order (text {:#x}+{:#x}, ro {:#x}+{:#x}, data {:#x}+{:#x})",
             m.name, m.text.offset, m.text.size, m.ro.offset, m.ro.size, m.data.offset, m.data.size);
    }
    if (m.ro.offset % kPage || m.data.offset % kPage) {
        fail("{}: .rodata and .data must be page aligned", m.name);
    }
}

void load_nso(ByteSpan file, Module& m, bool verify_hashes) {
    Reader r(file, "nso");
    r.require(0, 0x100);
    std::uint32_t flags = r.u32(0xC);
    m.nso_flags = flags;
    if (flags & (1u << 7)) {
        fail("{}: zstd-compressed segments (NSO flag bit 7, firmware 22.0.0+) are not supported yet", m.name);
    }
    NsoSegmentHeader seg[3] = {
        {r.u32(0x10), r.u32(0x14), r.u32(0x18), r.u32(0x60)},
        {r.u32(0x20), r.u32(0x24), r.u32(0x28), r.u32(0x64)},
        {r.u32(0x30), r.u32(0x34), r.u32(0x38), r.u32(0x68)},
    };
    const char* seg_names[3] = {"text", "rodata", "data"};
    std::uint32_t bss_size = r.u32(0x3C);
    for (int i = 0; i < 32; ++i) {
        m.module_id[i] = r.u8(0x40 + i);
    }
    for (int i = 0; i < 3; ++i) {
        bool compressed = (flags >> i) & 1;
        bool hashed = (flags >> (3 + i)) & 1;
        std::uint32_t stored = compressed ? seg[i].file_size : seg[i].size;
        ByteSpan raw = r.slice(seg[i].file_offset, stored);
        Bytes plain = compressed ? lz4::decompress_block(raw, seg[i].size) : Bytes(raw.begin(), raw.end());
        if (hashed && verify_hashes) {
            auto digest = sha256(plain);
            for (int b = 0; b < 32; ++b) {
                if (digest[b] != r.u8(0xA0 + 0x20 * i + b)) {
                    fail("{}: SHA-256 mismatch in .{} segment", m.name, seg_names[i]);
                }
            }
        }
        place(m.image, seg[i].mem_offset, plain);
    }
    m.text = {seg[0].mem_offset, seg[0].size};
    m.ro = {seg[1].mem_offset, seg[1].size};
    m.data = {seg[2].mem_offset, seg[2].size};
    m.bss = {m.data.end(), bss_size};
}

void load_nro(ByteSpan file, Module& m) {
    Reader r(file, "nro");
    r.require(0, 0x80);
    std::uint32_t flags = r.u32(0x1C);
    if (flags & 2) {
        fail("{}: compressed NRO files (firmware 23.0.0+) are not supported yet", m.name);
    }
    m.text = {r.u32(0x20), r.u32(0x24)};
    m.ro = {r.u32(0x28), r.u32(0x2C)};
    m.data = {r.u32(0x30), r.u32(0x34)};
    std::uint32_t bss_size = r.u32(0x38);
    for (int i = 0; i < 32; ++i) {
        m.module_id[i] = r.u8(0x40 + i);
    }
    ByteSpan body = r.slice(0, m.data.end());
    m.image.assign(body.begin(), body.end());
    m.bss = {m.data.end(), bss_size};
}

void load_mod0(Module& m) {
    Reader img(m.image, "image");
    std::uint32_t header_offset = img.u32(m.text.offset + 4);
    std::uint64_t at = m.text.offset + header_offset;
    if (!img.matches(at, "MOD0")) {
        fail("{}: no MOD0 header at {:#x} (word at .text+4 is {:#x})", m.name, at, header_offset);
    }
    auto rel = [&](std::uint64_t field) -> std::uint64_t {
        std::int64_t delta = img.s32(at + field);
        std::int64_t value = static_cast<std::int64_t>(at) + delta;
        if (value < 0) {
            fail("{}: MOD0 field {:#x} points before the module start", m.name, field);
        }
        return static_cast<std::uint64_t>(value);
    };
    m.mod0.offset = at;
    m.mod0.dynamic = rel(0x4);
    m.mod0.bss_start = rel(0x8);
    m.mod0.bss_end = rel(0xC);
    m.mod0.eh_frame_hdr_start = rel(0x10);
    m.mod0.eh_frame_hdr_end = rel(0x14);
    m.mod0.module_object = rel(0x18);
}

void load_dynamic(Module& m) {
    Reader img(m.image, "image");
    for (std::uint64_t at = m.mod0.dynamic;; at += sizeof(elf::Dyn)) {
        elf::Dyn d{img.read<std::int64_t>(at), img.u64(at + 8)};
        if (d.tag == elf::DT_NULL) {
            break;
        }
        m.dynamic.push_back(d);
    }
}

std::uint64_t count_symbols(const Module& m, ByteSpan file) {
    Reader img(m.image, "image");
    if (auto hash = m.dynamic_value(elf::DT_HASH)) {
        return img.u32(*hash + 4);
    }
    if (auto gnu = m.dynamic_value(elf::DT_GNU_HASH)) {
        std::uint32_t nbuckets = img.u32(*gnu);
        std::uint32_t symoffset = img.u32(*gnu + 4);
        std::uint32_t bloom_size = img.u32(*gnu + 8);
        std::uint64_t buckets = *gnu + 16 + std::uint64_t(bloom_size) * 8;
        std::uint64_t chains = buckets + std::uint64_t(nbuckets) * 4;
        std::uint32_t max_index = 0;
        for (std::uint32_t i = 0; i < nbuckets; ++i) {
            max_index = std::max(max_index, img.u32(buckets + i * 4));
        }
        if (max_index < symoffset) {
            return symoffset;
        }
        std::uint64_t index = max_index;
        while ((img.u32(chains + (index - symoffset) * 4) & 1) == 0) {
            ++index;
        }
        return index + 1;
    }
    if (m.format == ModuleFormat::Nso) {
        Reader r(file, "nso");
        return r.u32(0x9C) / sizeof(elf::Sym);
    }
    Reader r(file, "nro");
    return r.u32(0x7C) / sizeof(elf::Sym);
}

void load_symbols(Module& m, ByteSpan file) {
    auto symtab = m.dynamic_value(elf::DT_SYMTAB);
    auto strtab = m.dynamic_value(elf::DT_STRTAB);
    auto strsz = m.dynamic_value(elf::DT_STRSZ);
    if (!symtab || !strtab || !strsz) {
        fail("{}: dynamic section lacks DT_SYMTAB/DT_STRTAB/DT_STRSZ", m.name);
    }
    Reader img(m.image, "image");
    Reader strings(img.slice(*strtab, *strsz), "dynstr");
    std::uint64_t count = count_symbols(m, file);
    m.symbols.reserve(count);
    for (std::uint64_t i = 0; i < count; ++i) {
        std::uint64_t at = *symtab + i * sizeof(elf::Sym);
        Symbol s;
        std::uint32_t name = img.u32(at);
        s.info = img.u8(at + 4);
        s.other = img.u8(at + 5);
        s.shndx = img.u16(at + 6);
        s.value = img.u64(at + 8);
        s.size = img.u64(at + 16);
        s.name = name ? strings.cstr(name) : std::string();
        m.symbols.push_back(std::move(s));
    }
}

void check_reloc(const Module& m, const Reloc& rel) {
    if (rel.offset + 8 > m.image.size()) {
        fail("{}: relocation at {:#x} is outside the image", m.name, rel.offset);
    }
    if (rel.sym >= m.symbols.size()) {
        fail("{}: relocation at {:#x} references symbol {} of {}", m.name, rel.offset, rel.sym, m.symbols.size());
    }
}

void load_relocs(Module& m) {
    Reader img(m.image, "image");
    auto plt = m.dynamic_value(elf::DT_JMPREL);
    std::uint64_t plt_size = m.dynamic_value(elf::DT_PLTRELSZ).value_or(0);
    std::int64_t plt_kind = static_cast<std::int64_t>(m.dynamic_value(elf::DT_PLTREL).value_or(elf::DT_RELA));
    auto in_plt = [&](std::uint64_t at) { return plt && at >= *plt && at < *plt + plt_size; };

    auto add_rela = [&](std::uint64_t at, RelocSource source) {
        Reloc rel;
        rel.offset = img.u64(at);
        std::uint64_t info = img.u64(at + 8);
        rel.type = elf::r_type(info);
        rel.sym = elf::r_sym(info);
        rel.addend = img.read<std::int64_t>(at + 16);
        rel.source = source;
        check_reloc(m, rel);
        m.relocs.push_back(rel);
    };
    auto add_rel = [&](std::uint64_t at, RelocSource source) {
        Reloc rel;
        rel.offset = img.u64(at);
        std::uint64_t info = img.u64(at + 8);
        rel.type = elf::r_type(info);
        rel.sym = elf::r_sym(info);
        rel.source = source;
        check_reloc(m, rel);
        rel.addend = img.read<std::int64_t>(rel.offset);
        m.relocs.push_back(rel);
    };

    if (auto rela = m.dynamic_value(elf::DT_RELA)) {
        std::uint64_t size = m.dynamic_value(elf::DT_RELASZ).value_or(0);
        for (std::uint64_t at = *rela; at < *rela + size; at += sizeof(elf::Rela)) {
            if (!in_plt(at)) {
                add_rela(at, RelocSource::Rela);
            }
        }
    }
    if (auto rel = m.dynamic_value(elf::DT_REL)) {
        std::uint64_t size = m.dynamic_value(elf::DT_RELSZ).value_or(0);
        for (std::uint64_t at = *rel; at < *rel + size; at += sizeof(elf::Rel)) {
            if (!in_plt(at)) {
                add_rel(at, RelocSource::Rel);
            }
        }
    }
    if (auto relr = m.dynamic_value(elf::DT_RELR)) {
        std::uint64_t size = m.dynamic_value(elf::DT_RELRSZ).value_or(0);
        std::uint64_t where = 0;
        auto add = [&](std::uint64_t offset) {
            Reloc r;
            r.offset = offset;
            r.type = elf::R_AARCH64_RELATIVE;
            r.source = RelocSource::Relr;
            check_reloc(m, r);
            r.addend = img.read<std::int64_t>(offset);
            m.relocs.push_back(r);
        };
        for (std::uint64_t at = *relr; at < *relr + size; at += 8) {
            std::uint64_t entry = img.u64(at);
            if ((entry & 1) == 0) {
                where = entry;
                add(where);
                where += 8;
            } else {
                std::uint64_t bits = entry;
                for (int i = 0; (bits >>= 1) != 0; ++i) {
                    if (bits & 1) {
                        add(where + std::uint64_t(i) * 8);
                    }
                }
                where += 63 * 8;
            }
        }
    }
    if (plt) {
        std::uint64_t entry = plt_kind == elf::DT_REL ? sizeof(elf::Rel) : sizeof(elf::Rela);
        for (std::uint64_t at = *plt; at < *plt + plt_size; at += entry) {
            if (plt_kind == elf::DT_REL) {
                add_rel(at, RelocSource::Plt);
            } else {
                add_rela(at, RelocSource::Plt);
            }
        }
    }
}

} // namespace

std::optional<std::uint64_t> Module::dynamic_value(std::int64_t tag) const {
    for (const auto& d : dynamic) {
        if (d.tag == tag) {
            return d.val;
        }
    }
    return std::nullopt;
}

const Symbol* Module::find_export(const std::string& wanted) const {
    for (const auto& s : symbols) {
        if (s.defined() && s.name == wanted && s.bind() != elf::STB_LOCAL) {
            return &s;
        }
    }
    return nullptr;
}

std::size_t Module::import_count() const {
    std::size_t n = 0;
    for (std::size_t i = 1; i < symbols.size(); ++i) {
        n += !symbols[i].defined() && !symbols[i].name.empty();
    }
    return n;
}

std::size_t Module::export_count() const {
    std::size_t n = 0;
    for (std::size_t i = 1; i < symbols.size(); ++i) {
        n += symbols[i].defined() && symbols[i].bind() != elf::STB_LOCAL && !symbols[i].name.empty();
    }
    return n;
}

Module load_module(ByteSpan file, const std::string& name, bool verify_hashes) {
    require_module_file(file, name);
    Module m;
    m.name = name;
    if (detect_file_kind(file) == FileKind::Nso) {
        m.format = ModuleFormat::Nso;
        load_nso(file, m, verify_hashes);
    } else {
        m.format = ModuleFormat::Nro;
        load_nro(file, m);
    }
    check_segment_order(m);
    m.image.resize(align_up(std::max<std::uint64_t>(m.bss.end(), m.image.size()), kPage), 0);
    load_mod0(m);
    std::uint64_t end = std::max(m.bss.end(), m.mod0.bss_end);
    if (end - m.data.end() > (std::uint64_t(1) << 32)) {
        fail("{}: implausible .bss size {:#x}", m.name, end - m.data.end());
    }
    m.image.resize(align_up(std::max<std::uint64_t>(end, m.image.size()), kPage), 0);
    m.bss = {m.data.end(), end - m.data.end()};
    if (m.mod0.dynamic >= m.image.size()) {
        fail("{}: MOD0 dynamic offset {:#x} is outside the image", m.name, m.mod0.dynamic);
    }
    load_dynamic(m);
    load_symbols(m, file);
    load_relocs(m);
    m.init = m.dynamic_value(elf::DT_INIT).value_or(0);
    m.fini = m.dynamic_value(elf::DT_FINI).value_or(0);
    m.init_array = m.dynamic_value(elf::DT_INIT_ARRAY).value_or(0);
    m.init_array_size = m.dynamic_value(elf::DT_INIT_ARRAYSZ).value_or(0);
    m.fini_array = m.dynamic_value(elf::DT_FINI_ARRAY).value_or(0);
    m.fini_array_size = m.dynamic_value(elf::DT_FINI_ARRAYSZ).value_or(0);
    m.pltgot = m.dynamic_value(elf::DT_PLTGOT).value_or(0);
    m.has_hash = m.dynamic_value(elf::DT_HASH).has_value();
    m.has_gnu_hash = m.dynamic_value(elf::DT_GNU_HASH).has_value();
    return m;
}

} // namespace nx
