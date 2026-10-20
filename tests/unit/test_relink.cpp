// SPDX-License-Identifier: MIT
// Builds a small synthetic module in memory, writes it as NSO and NRO, loads
// it back, converts it and inspects the ELF the relinker produces.
#include "test.hpp"

#include "nxabi/nxabi.h"

#include "nx/common/sha256.hpp"
#include "nx/format/detect.hpp"
#include "nx/format/elf_reader.hpp"
#include "nx/format/lz4.hpp"
#include "nx/format/module.hpp"
#include "nx/relinker/a64.hpp"
#include "nx/relinker/census.hpp"
#include "nx/relinker/writer.hpp"

#include <cstring>

using namespace nx;

namespace {

constexpr std::uint64_t kText = 0x0, kTextSize = 0x1000;
constexpr std::uint64_t kRo = 0x1000, kRoSize = 0x1000;
constexpr std::uint64_t kData = 0x2000, kDataSize = 0x1000;
constexpr std::uint64_t kBssSize = 0x1000;
constexpr std::uint64_t kMod0 = 0x80;
constexpr std::uint64_t kCode = 0x100;
constexpr std::uint64_t kNnMain = kCode + 0x18;

constexpr std::uint64_t kDynsym = kRo + 0x000;
constexpr std::uint64_t kDynstr = kRo + 0x100;
constexpr std::uint64_t kHash = kRo + 0x200;
constexpr std::uint64_t kRela = kRo + 0x300;
constexpr std::uint64_t kJmprel = kRo + 0x400;
constexpr std::uint64_t kDynamic = kData + 0x000;
constexpr std::uint64_t kGot = kData + 0x200;
constexpr std::uint64_t kInitArray = kData + 0x300;

const char* const kSleep = "_ZN2nn2os11SleepThreadENS_8TimeSpanE";

struct Synthetic {
    Bytes image = Bytes(kData + kDataSize, 0);

    void u32(std::uint64_t at, std::uint32_t v) { store_le(image.data() + at, v); }
    void u64(std::uint64_t at, std::uint64_t v) { store_le(image.data() + at, v); }

    Synthetic() {
        u32(kText + 4, static_cast<std::uint32_t>(kMod0));
        std::memcpy(image.data() + kMod0, "MOD0", 4);
        auto rel = [&](std::uint64_t field, std::uint64_t target) {
            u32(kMod0 + field, static_cast<std::uint32_t>(static_cast<std::int64_t>(target) - std::int64_t(kMod0)));
        };
        rel(0x4, kDynamic);
        rel(0x8, kData + kDataSize);
        rel(0xC, kData + kDataSize + kBssSize);
        rel(0x10, kRo + 0x800);
        rel(0x14, kRo + 0x800);
        rel(0x18, kData + kDataSize);

        u32(kCode + 0x00, 0xD40004E1);                 // svc #0x27
        u32(kCode + 0x04, a64::mrs(5, a64::kTpidrroEl0)); // mrs x5, tpidrro_el0
        u32(kCode + 0x08, a64::mrs(6, a64::kCntvctEl0));  // mrs x6, cntvct_el0
        u32(kCode + 0x0C, 0xC85F7C20);                 // ldxr x0, [x1]
        u32(kCode + 0x10, a64::mrs(31, a64::kCntfrqEl0)); // mrs xzr, cntfrq_el0
        u32(kCode + 0x14, a64::msr(a64::kTpidrEl0, 16));  // msr tpidr_el0, x16
        u32(kNnMain, a64::kRet);

        // dynstr
        const char* names[] = {"", "puts", "nnMain", kSleep, "missing_data"};
        std::uint32_t name_at[5] = {};
        std::uint64_t at = kDynstr + 1;
        for (int i = 1; i < 5; ++i) {
            name_at[i] = static_cast<std::uint32_t>(at - kDynstr);
            std::memcpy(image.data() + at, names[i], std::strlen(names[i]) + 1);
            at += std::strlen(names[i]) + 1;
        }
        std::uint64_t strsz = at - kDynstr;
        // dynsym
        auto sym = [&](int i, std::uint8_t info, std::uint16_t shndx, std::uint64_t value) {
            elf::Sym s{name_at[i], info, 0, shndx, value, 0};
            std::memcpy(image.data() + kDynsym + i * sizeof(elf::Sym), &s, sizeof s);
        };
        sym(1, elf::st_info(elf::STB_GLOBAL, elf::STT_FUNC), 0, 0);
        sym(2, elf::st_info(elf::STB_GLOBAL, elf::STT_FUNC), 7, kNnMain);
        sym(3, elf::st_info(elf::STB_GLOBAL, elf::STT_FUNC), 0, 0);
        sym(4, elf::st_info(elf::STB_GLOBAL, elf::STT_OBJECT), 0, 0);
        // hash: one bucket, chain through all symbols
        u32(kHash, 1);
        u32(kHash + 4, 5);
        u32(kHash + 8, 4);
        u32(kHash + 12 + 4 * 4, 3);
        u32(kHash + 12 + 3 * 4, 2);
        u32(kHash + 12 + 2 * 4, 1);
        // rela: RELATIVE pointer in data, init_array entry, GLOB_DAT of missing_data
        auto rela = [&](std::uint64_t slot, std::uint64_t where, std::uint32_t symidx, std::uint32_t type,
                        std::int64_t addend) {
            elf::Rela r{where, elf::r_info(symidx, type), addend};
            std::memcpy(image.data() + slot, &r, sizeof r);
        };
        rela(kRela + 0, kGot + 0x00, 0, elf::R_AARCH64_RELATIVE, kNnMain);
        rela(kRela + 24, kInitArray, 0, elf::R_AARCH64_RELATIVE, kNnMain);
        rela(kRela + 48, kGot + 0x08, 4, elf::R_AARCH64_GLOB_DAT, 0);
        rela(kJmprel + 0, kGot + 0x10, 1, elf::R_AARCH64_JUMP_SLOT, 0);
        rela(kJmprel + 24, kGot + 0x18, 3, elf::R_AARCH64_JUMP_SLOT, 0);
        // dynamic
        std::uint64_t d = kDynamic;
        auto dyn = [&](std::int64_t tag, std::uint64_t val) {
            u64(d, static_cast<std::uint64_t>(tag));
            u64(d + 8, val);
            d += 16;
        };
        dyn(elf::DT_HASH, kHash);
        dyn(elf::DT_STRTAB, kDynstr);
        dyn(elf::DT_SYMTAB, kDynsym);
        dyn(elf::DT_STRSZ, strsz);
        dyn(elf::DT_SYMENT, 24);
        dyn(elf::DT_RELA, kRela);
        dyn(elf::DT_RELASZ, 72);
        dyn(elf::DT_RELAENT, 24);
        dyn(elf::DT_JMPREL, kJmprel);
        dyn(elf::DT_PLTRELSZ, 48);
        dyn(elf::DT_PLTREL, elf::DT_RELA);
        dyn(elf::DT_PLTGOT, kGot);
        dyn(elf::DT_INIT_ARRAY, kInitArray);
        dyn(elf::DT_INIT_ARRAYSZ, 8);
        dyn(elf::DT_NULL, 0);
    }

    ByteSpan seg(std::uint64_t off, std::uint64_t size) const { return ByteSpan(image).subspan(off, size); }

    Bytes as_nso(bool compress) const {
        Bytes out(0x100, 0);
        std::memcpy(out.data(), "NSO0", 4);
        std::uint32_t flags = compress ? 0x3F : 0x38;
        store_le<std::uint32_t>(out.data() + 0xC, flags);
        const std::uint64_t segs[3][2] = {{kText, kTextSize}, {kRo, kRoSize}, {kData, kDataSize}};
        const std::uint64_t header_at[3] = {0x10, 0x20, 0x30};
        for (int i = 0; i < 3; ++i) {
            ByteSpan plain = seg(segs[i][0], segs[i][1]);
            Bytes stored = compress ? lz4::compress_block(plain) : Bytes(plain.begin(), plain.end());
            std::uint32_t file_offset = static_cast<std::uint32_t>(out.size());
            store_le<std::uint32_t>(out.data() + header_at[i], file_offset);
            store_le<std::uint32_t>(out.data() + header_at[i] + 4, static_cast<std::uint32_t>(segs[i][0]));
            store_le<std::uint32_t>(out.data() + header_at[i] + 8, static_cast<std::uint32_t>(segs[i][1]));
            store_le<std::uint32_t>(out.data() + 0x60 + 4 * i, static_cast<std::uint32_t>(stored.size()));
            auto digest = sha256(plain);
            std::memcpy(out.data() + 0xA0 + 0x20 * i, digest.data(), 32);
            out.insert(out.end(), stored.begin(), stored.end());
        }
        store_le<std::uint32_t>(out.data() + 0x3C, static_cast<std::uint32_t>(kBssSize));
        out[0x40] = 0xAB;
        out[0x41] = 0xCD;
        return out;
    }

    Bytes as_nro() const {
        Bytes out = image;
        store_le<std::uint32_t>(out.data() + 0x10, 0x304F524E); // "NRO0"
        store_le<std::uint32_t>(out.data() + 0x18, static_cast<std::uint32_t>(out.size()));
        store_le<std::uint32_t>(out.data() + 0x20, static_cast<std::uint32_t>(kText));
        store_le<std::uint32_t>(out.data() + 0x24, static_cast<std::uint32_t>(kTextSize));
        store_le<std::uint32_t>(out.data() + 0x28, static_cast<std::uint32_t>(kRo));
        store_le<std::uint32_t>(out.data() + 0x2C, static_cast<std::uint32_t>(kRoSize));
        store_le<std::uint32_t>(out.data() + 0x30, static_cast<std::uint32_t>(kData));
        store_le<std::uint32_t>(out.data() + 0x34, static_cast<std::uint32_t>(kDataSize));
        store_le<std::uint32_t>(out.data() + 0x38, static_cast<std::uint32_t>(kBssSize));
        return out;
    }
};

std::uint64_t branch_target(std::uint64_t pc, std::uint32_t insn) {
    std::int64_t imm = static_cast<std::int64_t>(static_cast<std::int32_t>(insn << 6) >> 6) * 4;
    return pc + static_cast<std::uint64_t>(imm);
}

std::uint32_t word_at(const Bytes& elf, std::uint64_t at) { return load_le<std::uint32_t>(elf.data() + at); }

} // namespace

NX_TEST(module_loads_from_nso_and_nro) {
    Synthetic s;
    for (const Bytes& file : {s.as_nso(true), s.as_nso(false), s.as_nro()}) {
        Module m = load_module(file, "main");
        NX_CHECK_EQ(m.text.size, kTextSize);
        NX_CHECK_EQ(m.data.offset, kData);
        NX_CHECK_EQ(m.bss.size, kBssSize);
        NX_CHECK_EQ(m.image.size(), kData + kDataSize + kBssSize);
        NX_CHECK_EQ(m.mod0.offset, kMod0);
        NX_CHECK_EQ(m.mod0.dynamic, kDynamic);
        NX_CHECK_EQ(m.symbols.size(), 5);
        NX_CHECK(m.symbols[3].name == kSleep);
        NX_CHECK_EQ(m.import_count(), 3);
        NX_CHECK_EQ(m.export_count(), 1);
        NX_CHECK_EQ(m.relocs.size(), 5);
        NX_CHECK_EQ(m.init_array, kInitArray);
        NX_CHECK(m.find_export("nnMain") && m.find_export("nnMain")->value == kNnMain);
    }
}

NX_TEST(module_rejects_bad_input) {
    Synthetic s;
    Bytes nso = s.as_nso(true);
    nso[nso.size() - 1] ^= 0xFF; // corrupt the data segment
    NX_CHECK_THROWS(load_module(nso, "main"));
    Bytes container(0x400, 0);
    std::memcpy(container.data(), "PFS0", 4);
    NX_CHECK(detect_file_kind(container) == FileKind::PackagedContainer);
    NX_CHECK_THROWS(load_module(container, "main"));
    Bytes random(0x400, 0x5A);
    NX_CHECK_THROWS(load_module(random, "main"));
}

NX_TEST(census_counts_privileged_instructions) {
    Synthetic s;
    Census c = run_census(load_module(s.as_nso(false), "main"));
    NX_CHECK_EQ(c.svc_by_number.at(0x27), 1);
    NX_CHECK_EQ(c.sysreg_reads.at("tpidrro_el0"), 1);
    NX_CHECK_EQ(c.sysreg_reads.at("cntvct_el0"), 1);
    NX_CHECK_EQ(c.sysreg_writes.at("tpidr_el0"), 1);
    NX_CHECK_EQ(c.exclusive_unordered, 1);
    NX_CHECK_EQ(c.relocations.at("R_AARCH64_JUMP_SLOT"), 2);
}

NX_TEST(convert_main_module) {
    Synthetic s;
    Module m = load_module(s.as_nso(true), "main");
    Registry registry;
    registry.add("puts");
    ConvertOptions opt;
    opt.needed = {"libnxrt.so"};
    opt.registry = &registry;
    ConvertResult r = convert_module(m, opt);

    NX_CHECK_EQ(r.report.trapped.size(), 1);
    NX_CHECK(r.report.trapped[0] == kSleep);
    NX_CHECK_EQ(r.report.unresolved_data.size(), 1);
    NX_CHECK_EQ(r.report.patches.at("svc"), 1);
    NX_CHECK_EQ(r.report.patches.at("nop"), 1);

    ElfFile f = read_elf(r.elf);
    NX_CHECK_EQ(f.header.type, elf::ET_DYN);
    NX_CHECK_EQ(f.header.machine, elf::EM_AARCH64);
    const elf::Phdr* tls = f.find_segment(elf::PT_TLS);
    NX_CHECK(tls && tls->memsz == NXABI_TLS_SIZE && tls->align == NXABI_TLS_ALIGN);
    NX_CHECK(f.find_segment(elf::PT_INTERP));
    for (const auto& p : f.segments) {
        if (p.type == elf::PT_LOAD) {
            NX_CHECK_EQ(p.offset, p.vaddr);
            NX_CHECK_EQ(p.vaddr % 0x1000, 0);
        }
    }
    const elf::Phdr* info_ph = f.find_segment(NXABI_PT_MODULE_INFO);
    NX_CHECK(info_ph);
    nxabi_module_info info{};
    std::memcpy(&info, r.elf.data() + info_ph->offset, sizeof info);
    NX_CHECK_EQ(info.magic, NXABI_MODULE_INFO_MAGIC);
    NX_CHECK_EQ(info.abi_version, NXABI_VERSION);
    const std::uint64_t H = info.image_offset;
    NX_CHECK_EQ(H % 0x10000, 0);
    NX_CHECK_EQ(info.nnmain_offset, H + kNnMain);
    NX_CHECK_EQ(info.init_array_offset, H + kInitArray);
    NX_CHECK_EQ(info.init_array_count, 1);
    NX_CHECK_EQ(info.trap_count, 1);
    NX_CHECK(info.flags & NXABI_MODULE_MAIN);

    bool saw_prefixed = false, saw_entry = false;
    for (const auto& sym : f.dynamic_symbols) {
        saw_prefixed |= sym.name == "nx$nnMain" && sym.value == H + kNnMain && sym.shndx != elf::SHN_UNDEF;
        saw_entry |= sym.name == NXABI_ENTRY_SYMBOL && sym.shndx == elf::SHN_UNDEF;
    }
    NX_CHECK(saw_prefixed);
    NX_CHECK(saw_entry);

    // ldxr became ldaxr, the xzr read became nop
    NX_CHECK_EQ(word_at(r.elf, H + kCode + 0x0C), 0xC85FFC20);
    NX_CHECK_EQ(word_at(r.elf, H + kCode + 0x10), a64::kNop);
    // tpidrro_el0 read branches to a stub that loads the TLR slot and returns
    std::uint64_t site = H + kCode + 0x04;
    std::uint64_t stub = branch_target(site, word_at(r.elf, site));
    NX_CHECK(stub < H);
    NX_CHECK_EQ(word_at(r.elf, stub), a64::mrs(5, a64::kTpidrEl0));
    NX_CHECK_EQ(word_at(r.elf, stub + 4), a64::ldr_x(5, 5, NXABI_TLS_TP_OFFSET + NXABI_TLS_SLOT_TLR));
    NX_CHECK_EQ(branch_target(stub + 8, word_at(r.elf, stub + 8)), site + 4);
    // svc stub saves x16/x17 first
    std::uint64_t svc_site = H + kCode;
    std::uint64_t svc_stub = branch_target(svc_site, word_at(r.elf, svc_site));
    NX_CHECK_EQ(word_at(r.elf, svc_stub), a64::stp_x_pre(16, 17, a64::kSp, -16));
    NX_CHECK_EQ(word_at(r.elf, svc_stub + 4), a64::movz_w(16, 0x27, 0));

    // relocations: RELATIVE rebased by H, missing JUMP_SLOT turned into RELATIVE to a trap stub
    const elf::Shdr* rela = f.find_section(".rela.dyn");
    NX_CHECK(rela);
    bool saw_relative = false, saw_trap = false;
    for (std::uint64_t at = rela->offset; at < rela->offset + rela->size; at += sizeof(elf::Rela)) {
        elf::Rela e{};
        std::memcpy(&e, r.elf.data() + at, sizeof e);
        if (e.offset == H + kGot && elf::r_type(e.info) == elf::R_AARCH64_RELATIVE) {
            saw_relative = e.addend == static_cast<std::int64_t>(H + kNnMain);
        }
        if (e.offset == H + kGot + 0x18 && elf::r_type(e.info) == elf::R_AARCH64_RELATIVE) {
            std::uint64_t trap = static_cast<std::uint64_t>(e.addend);
            saw_trap = trap < H && word_at(r.elf, trap) == a64::movz_w(17, 0, 0);
        }
    }
    NX_CHECK(saw_relative);
    NX_CHECK(saw_trap);

    bool bind_now = false;
    for (const auto& d : f.dynamic) {
        bind_now |= d.tag == elf::DT_FLAGS && (d.val & elf::DF_BIND_NOW);
    }
    NX_CHECK(bind_now);
}

NX_TEST(convert_shared_module_without_patching) {
    Synthetic s;
    Module m = load_module(s.as_nro(), "subsdk0");
    ConvertOptions opt;
    opt.kind = OutputKind::Shared;
    opt.module_name = "subsdk0";
    opt.soname = "subsdk0.so";
    opt.patch = false;
    ConvertResult r = convert_module(m, opt);
    ElfFile f = read_elf(r.elf);
    NX_CHECK(!f.find_segment(elf::PT_INTERP));
    NX_CHECK(!f.find_segment(elf::PT_TLS));
    NX_CHECK_EQ(f.header.entry, 0);
    const elf::Phdr* info_ph = f.find_segment(NXABI_PT_MODULE_INFO);
    nxabi_module_info info{};
    std::memcpy(&info, r.elf.data() + info_ph->offset, sizeof info);
    NX_CHECK_EQ(word_at(r.elf, info.image_offset + kCode), 0xD40004E1); // svc left alone
    NX_CHECK_EQ(info.trap_count, 0);
}
