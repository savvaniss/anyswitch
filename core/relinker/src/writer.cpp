// SPDX-License-Identifier: MIT
// Output layout (all PT_LOAD segments use file offset == vaddr, 64 KiB aligned):
//
//   [0]      R    ELF header, program headers, .interp, .dynsym, .dynstr, .hash,
//                 .rela.dyn, .rela.plt, module info, trap names, npdm copy
//   [B]      R X  stubs: entry, shared handlers, per-site patch stubs, trap stubs
//   [H]      ...  the original module image, shifted by H (text RX, rodata R, data+bss RW)
//   [C]      RW   .dynamic, entry GOT slot, nxabi_module_rw, TLS block (main only)
//
// Keeping the original image contiguous preserves every PC-relative reference
// inside it; only absolute values (relocation addends, symbol values) move by H.
#include "nx/relinker/writer.hpp"

#include "nxabi/nxabi.h"

#include "nx/format/elf.hpp"
#include "nx/relinker/a64.hpp"

#include <algorithm>
#include <cstring>
#include <unordered_map>

namespace nx {
namespace {

constexpr std::uint64_t kSegAlign = 0x10000;

class StringTable {
public:
    StringTable() { data_.push_back(0); }
    std::uint32_t add(const std::string& s) {
        if (s.empty()) {
            return 0;
        }
        auto it = index_.find(s);
        if (it != index_.end()) {
            return it->second;
        }
        auto at = static_cast<std::uint32_t>(data_.size());
        data_.insert(data_.end(), s.begin(), s.end());
        data_.push_back(0);
        index_.emplace(s, at);
        return at;
    }
    const Bytes& bytes() const { return data_; }

private:
    Bytes data_;
    std::unordered_map<std::string, std::uint32_t> index_;
};

class Code {
public:
    explicit Code(std::uint64_t base) : base_(base) {}
    std::uint64_t pc() const { return base_ + words_.size() * 4; }
    void emit(std::uint32_t word) { words_.push_back(word); }
    std::uint64_t size_bytes() const { return words_.size() * 4; }
    const std::vector<std::uint32_t>& words() const { return words_; }

private:
    std::uint64_t base_;
    std::vector<std::uint32_t> words_;
};

struct StubTargets {
    std::uint64_t info = 0;
    std::uint64_t rw = 0;
    std::uint64_t got_entry = 0;
    std::uint64_t image = 0;
};

struct StubPlan {
    std::uint64_t entry = 0;
    std::uint64_t svc_common = 0;
    std::uint64_t tick_common = 0;
    std::uint64_t trap_common = 0;
    std::vector<std::uint64_t> site_stub;
    std::vector<std::uint64_t> trap_stub;
};

void emit_adrp_add(Code& c, std::uint32_t rd, std::uint64_t target) {
    c.emit(a64::adrp(rd, c.pc(), target));
    c.emit(a64::add_imm(rd, rd, static_cast<std::uint32_t>(target & 0xFFF)));
}

Code emit_stubs(std::uint64_t base, const StubTargets& t, const std::vector<PatchSite>& sites,
                std::size_t trap_count, bool is_main, StubPlan& plan) {
    using namespace a64;
    Code c(base);
    plan = StubPlan{};
    plan.site_stub.assign(sites.size(), 0);
    plan.trap_stub.assign(trap_count, 0);

    if (is_main) {
        // ld.so enters with x0 = rtld_fini and sp = initial process stack.
        plan.entry = c.pc();
        c.emit(mov_reg(2, 0));
        c.emit(mov_from_sp(0));
        emit_adrp_add(c, 1, t.info);
        c.emit(adrp(16, c.pc(), t.got_entry));
        c.emit(ldr_x(16, 16, static_cast<std::uint32_t>(t.got_entry & 0xFFF)));
        c.emit(br(16));
    }

    bool has_svc = std::any_of(sites.begin(), sites.end(), [](auto& s) { return s.kind == PatchKind::Svc; });
    bool has_tick = std::any_of(sites.begin(), sites.end(), [](auto& s) { return s.kind == PatchKind::ReadTick; });

    if (has_svc) {
        // In: w16 = svc number, x17 = guest return pc, lr = per-site stub.
        // Calls svc_handler(regs[8], number, pc); x0-x7 are copied back as results.
        plan.svc_common = c.pc();
        c.emit(sub_imm(kSp, kSp, 96));
        c.emit(stp_x(0, 1, kSp, 0));
        c.emit(stp_x(2, 3, kSp, 16));
        c.emit(stp_x(4, 5, kSp, 32));
        c.emit(stp_x(6, 7, kSp, 48));
        c.emit(stp_x(18, 30, kSp, 64));
        c.emit(mrs(9, kNzcv));
        c.emit(str_x(9, kSp, 80));
        c.emit(mov_from_sp(0));
        c.emit(mov_reg_w(1, 16));
        c.emit(mov_reg(2, 17));
        emit_adrp_add(c, 9, t.rw);
        c.emit(ldr_x(9, 9, NXABI_RW_SVC_HANDLER_OFFSET));
        c.emit(blr(9));
        c.emit(ldr_x(9, kSp, 80));
        c.emit(msr(kNzcv, 9));
        c.emit(ldp_x(18, 30, kSp, 64));
        c.emit(ldp_x(6, 7, kSp, 48));
        c.emit(ldp_x(4, 5, kSp, 32));
        c.emit(ldp_x(2, 3, kSp, 16));
        c.emit(ldp_x(0, 1, kSp, 0));
        c.emit(add_imm(kSp, kSp, 96));
        c.emit(kRet);
    }

    if (has_tick) {
        // Out: x0 = host counter scaled to 19.2 MHz. Preserves everything else
        // except lr (callers save it) and leaves NZCV untouched.
        plan.tick_common = c.pc();
        c.emit(stp_x_pre(1, 2, kSp, -16));
        c.emit(mrs(0, kCntvctEl0));
        emit_adrp_add(c, 1, t.rw);
        c.emit(ldp_x(1, 2, 1, NXABI_RW_TICK_MUL_INT_OFFSET));
        c.emit(umulh(2, 0, 2));
        c.emit(mul(1, 0, 1));
        c.emit(add_reg(0, 1, 2));
        c.emit(ldp_x_post(1, 2, kSp, 16));
        c.emit(kRet);
    }

    if (trap_count > 0) {
        // In: w17 = trap index. Calls trap_handler(info, index), which never returns.
        plan.trap_common = c.pc();
        emit_adrp_add(c, 0, t.info);
        c.emit(mov_reg_w(1, 17));
        emit_adrp_add(c, 16, t.rw);
        c.emit(ldr_x(16, 16, NXABI_RW_TRAP_HANDLER_OFFSET));
        c.emit(br(16));
    }

    for (std::size_t i = 0; i < sites.size(); ++i) {
        const PatchSite& s = sites[i];
        if (!patch_needs_stub(s)) {
            continue;
        }
        plan.site_stub[i] = c.pc();
        const std::uint64_t back = t.image + s.offset + 4;
        const std::uint32_t n = s.reg;
        switch (s.kind) {
        case PatchKind::Svc:
            c.emit(stp_x_pre(16, 17, kSp, -16));
            c.emit(movz_w(16, s.svc, 0));
            emit_adrp_add(c, 17, back);
            c.emit(str_x_pre(30, kSp, -16));
            c.emit(bl(c.pc(), plan.svc_common));
            c.emit(ldr_x_post(30, kSp, 16));
            c.emit(ldp_x_post(16, 17, kSp, 16));
            break;
        case PatchKind::ReadTlr:
            c.emit(mrs(n, kTpidrEl0));
            c.emit(ldr_x(n, n, NXABI_TLS_TP_OFFSET + NXABI_TLS_SLOT_TLR));
            break;
        case PatchKind::ReadGuestTp:
            c.emit(mrs(n, kTpidrEl0));
            c.emit(ldr_x(n, n, NXABI_TLS_TP_OFFSET + NXABI_TLS_SLOT_GUEST_TP));
            break;
        case PatchKind::WriteGuestTp: {
            std::uint32_t scratch = n == 16 ? 17 : 16;
            c.emit(str_x_pre(scratch, kSp, -16));
            c.emit(mrs(scratch, kTpidrEl0));
            c.emit(str_x(n, scratch, NXABI_TLS_TP_OFFSET + NXABI_TLS_SLOT_GUEST_TP));
            c.emit(ldr_x_post(scratch, kSp, 16));
            break;
        }
        case PatchKind::ReadTick:
            if (n == 0) {
                c.emit(str_x_pre(30, kSp, -16));
                c.emit(bl(c.pc(), plan.tick_common));
                c.emit(ldr_x_post(30, kSp, 16));
            } else if (n == 30) {
                c.emit(str_x_pre(0, kSp, -16));
                c.emit(bl(c.pc(), plan.tick_common));
                c.emit(mov_reg(30, 0));
                c.emit(ldr_x_post(0, kSp, 16));
            } else {
                c.emit(stp_x_pre(0, 30, kSp, -16));
                c.emit(bl(c.pc(), plan.tick_common));
                c.emit(mov_reg(n, 0));
                c.emit(ldp_x_post(0, 30, kSp, 16));
            }
            break;
        case PatchKind::ReadTickFreq:
            c.emit(movz_x(n, NXABI_GUEST_TICK_HZ & 0xFFFF, 0));
            c.emit(movk_x(n, NXABI_GUEST_TICK_HZ >> 16, 16));
            break;
        case PatchKind::OrderedExclusive:
        case PatchKind::Nop:
            break;
        }
        c.emit(b(c.pc(), back));
    }

    for (std::size_t i = 0; i < trap_count; ++i) {
        plan.trap_stub[i] = c.pc();
        c.emit(movz_w(17, static_cast<std::uint32_t>(i & 0xFFFF), 0));
        c.emit(movk_w(17, static_cast<std::uint32_t>(i >> 16), 16));
        c.emit(b(c.pc(), plan.trap_common));
    }
    return c;
}

enum class RelocPlan { Drop, Relative, Symbolic, Plt, Trap };

struct SectionSpec {
    std::string name;
    std::uint32_t type = 0;
    std::uint64_t flags = 0;
    std::uint64_t addr = 0;
    std::uint64_t offset = 0;
    std::uint64_t size = 0;
    std::uint32_t link = 0;
    std::uint32_t info = 0;
    std::uint64_t align = 1;
    std::uint64_t entsize = 0;
};

} // namespace

ConvertResult convert_module(const Module& m, const ConvertOptions& opt) {
    ConvertResult result;
    ConvertReport& rep = result.report;
    const bool is_main = opt.kind == OutputKind::Main;
    const std::string prefix = NXABI_SYMBOL_PREFIX;

    // ---- patch sites ---------------------------------------------------------
    std::vector<PatchSite> sites;
    if (opt.patch) {
        sites = find_patch_sites(m, opt.scan);
    }
    for (const auto& s : sites) {
        ++rep.patches[patch_kind_name(s.kind)];
    }

    // ---- imports and traps ---------------------------------------------------
    std::vector<bool> called(m.symbols.size(), false);
    for (const auto& r : m.relocs) {
        if (r.type == elf::R_AARCH64_JUMP_SLOT) {
            called[r.sym] = true;
        }
    }
    std::vector<std::int64_t> trap_of(m.symbols.size(), -1);
    std::vector<std::string> trap_names;
    for (std::size_t i = 1; i < m.symbols.size(); ++i) {
        const Symbol& s = m.symbols[i];
        if (s.defined() || s.name.empty()) {
            continue;
        }
        ++rep.imports;
        if (!opt.registry) {
            continue;
        }
        bool available = opt.registry->contains(s.name) || opt.sibling_exports.count(s.name) != 0;
        if (available || s.bind() == elf::STB_WEAK) {
            continue;
        }
        if (called[i]) {
            trap_of[i] = static_cast<std::int64_t>(trap_names.size());
            trap_names.push_back(s.name);
        } else {
            rep.unresolved_data.push_back(s.name);
        }
    }
    rep.trapped = trap_names;
    rep.exports = m.export_count();

    // ---- relocation plan -----------------------------------------------------
    std::vector<RelocPlan> plan(m.relocs.size(), RelocPlan::Drop);
    std::size_t n_dyn = is_main ? 1 : 0; // entry GOT slot
    std::size_t n_plt = 0;
    for (std::size_t i = 0; i < m.relocs.size(); ++i) {
        const Reloc& r = m.relocs[i];
        switch (r.type) {
        case elf::R_AARCH64_NONE: plan[i] = RelocPlan::Drop; break;
        case elf::R_AARCH64_RELATIVE: plan[i] = RelocPlan::Relative; break;
        case elf::R_AARCH64_ABS64:
        case elf::R_AARCH64_GLOB_DAT:
            plan[i] = trap_of[r.sym] >= 0 ? RelocPlan::Trap : RelocPlan::Symbolic;
            break;
        case elf::R_AARCH64_JUMP_SLOT:
            plan[i] = trap_of[r.sym] >= 0 ? RelocPlan::Trap : RelocPlan::Plt;
            break;
        default:
            fail("{}: unsupported relocation {} ({}) at {:#x}", m.name, elf::reloc_name(r.type), r.type, r.offset);
        }
        if (plan[i] == RelocPlan::Plt) {
            ++n_plt;
        } else if (plan[i] != RelocPlan::Drop) {
            ++n_dyn;
        }
    }
    rep.relocations = n_dyn + n_plt;

    // ---- symbols and strings ---------------------------------------------------
    StringTable dynstr;
    std::vector<std::uint32_t> needed_names;
    for (const auto& lib : opt.needed) {
        needed_names.push_back(dynstr.add(lib));
    }
    std::uint32_t soname_name = opt.kind == OutputKind::Shared ? dynstr.add(opt.soname) : 0;
    std::uint32_t runpath_name = dynstr.add(opt.runpath);
    std::vector<std::uint32_t> sym_names(m.symbols.size(), 0);
    for (std::size_t i = 1; i < m.symbols.size(); ++i) {
        if (!m.symbols[i].name.empty()) {
            sym_names[i] = dynstr.add(prefix + m.symbols[i].name);
        }
    }
    std::uint32_t entry_sym = 0;
    std::uint32_t entry_name = 0;
    std::size_t nsyms = m.symbols.size();
    if (is_main) {
        entry_sym = static_cast<std::uint32_t>(nsyms++);
        entry_name = dynstr.add(NXABI_ENTRY_SYMBOL);
    }
    const std::size_t nbucket = std::max<std::size_t>(1, nsyms / 2) | 1;

    Bytes trap_blob;
    if (!trap_names.empty()) {
        Writer w;
        std::vector<std::uint32_t> offsets;
        std::uint32_t at = static_cast<std::uint32_t>(trap_names.size() * 4);
        for (const auto& n : trap_names) {
            offsets.push_back(at);
            at += static_cast<std::uint32_t>(n.size() + 1);
        }
        for (auto o : offsets) {
            w.put<std::uint32_t>(o);
        }
        for (const auto& n : trap_names) {
            w.put_string(n);
        }
        trap_blob = std::move(w.bytes());
    }
    const bool has_npdm = is_main && !opt.npdm_raw.empty();

    // ---- program headers -------------------------------------------------------
    const bool has_ro = m.ro.size > 0;
    const bool has_eh = m.mod0.eh_frame_hdr_end > m.mod0.eh_frame_hdr_start;
    std::size_t nphdr = 1 /*PHDR*/ + (is_main ? 1 : 0) /*INTERP*/ + 2 /*A, B*/ + 1 /*text*/ + (has_ro ? 1 : 0) +
                        1 /*data*/ + 1 /*C*/ + 1 /*DYNAMIC*/ + (is_main ? 1 : 0) /*TLS*/ + (has_eh ? 1 : 0) +
                        1 /*GNU_STACK*/ + 1 /*module info*/;

    // ---- segment A layout ------------------------------------------------------
    std::uint64_t cursor = sizeof(elf::Ehdr) + nphdr * sizeof(elf::Phdr);
    auto reserve = [&](std::uint64_t size, std::uint64_t align) {
        cursor = align_up(cursor, align);
        std::uint64_t at = cursor;
        cursor += size;
        return at;
    };
    const std::uint64_t interp_at = is_main ? reserve(opt.interpreter.size() + 1, 1) : 0;
    const std::uint64_t dynsym_at = reserve(nsyms * sizeof(elf::Sym), 8);
    // dynstr grows below once all strings are known; reserve after it is final.
    const Bytes& dynstr_bytes = dynstr.bytes();
    const std::uint64_t dynstr_at = reserve(dynstr_bytes.size(), 1);
    const std::uint64_t hash_at = reserve((2 + nbucket + nsyms) * 4, 8);
    const std::uint64_t rela_at = reserve(n_dyn * sizeof(elf::Rela), 8);
    const std::uint64_t plt_at = reserve(n_plt * sizeof(elf::Rela), 8);
    const std::uint64_t info_at = reserve(sizeof(nxabi_module_info), 8);
    const std::uint64_t traps_at = trap_blob.empty() ? 0 : reserve(trap_blob.size(), 8);
    const std::uint64_t npdm_at = has_npdm ? reserve(opt.npdm_raw.size(), 8) : 0;
    const std::uint64_t a_end = cursor;

    // ---- segment B (stubs): size first, then H ----------------------------------
    const std::uint64_t b_at = align_up(a_end, kSegAlign);
    StubPlan stub_plan;
    StubTargets dry{b_at, b_at, b_at, b_at};
    std::uint64_t stub_size = emit_stubs(b_at, dry, sites, trap_names.size(), is_main, stub_plan).size_bytes();
    const std::uint64_t H = align_up(b_at + std::max<std::uint64_t>(stub_size, 4), kSegAlign);

    // ---- segment C layout ------------------------------------------------------
    const bool textrel = std::any_of(m.relocs.begin(), m.relocs.end(), [&](const Reloc& r) {
        return m.text.contains(r.offset) || m.ro.contains(r.offset);
    });
    rep.text_relocations = textrel;
    std::size_t ndyn = needed_names.size() + (opt.kind == OutputKind::Shared ? 1 : 0) +
                       (opt.runpath.empty() ? 0 : 1) + 5 /*HASH STRTAB SYMTAB STRSZ SYMENT*/ +
                       (n_dyn ? 3 : 0) + (n_plt ? (m.pltgot ? 4 : 3) : 0) + 1 /*BIND_NOW*/ + 1 /*FLAGS*/ +
                       (textrel ? 1 : 0) + 1 /*FLAGS_1*/ + (is_main ? 1 : 0) /*DEBUG*/ + 1 /*NULL*/;
    const std::uint64_t c_at = align_up(H + m.image.size(), kSegAlign);
    std::uint64_t c_cursor = c_at;
    const std::uint64_t dynamic_at = c_cursor;
    c_cursor += ndyn * sizeof(elf::Dyn);
    const std::uint64_t got_at = is_main ? align_up(c_cursor, 8) : 0;
    if (is_main) {
        c_cursor = got_at + 8;
    }
    const std::uint64_t rw_at = align_up(c_cursor, 64);
    c_cursor = rw_at + sizeof(nxabi_module_rw);
    const std::uint64_t c_filesz = c_cursor - c_at;
    const std::uint64_t tls_at = is_main ? align_up(c_cursor, NXABI_TLS_ALIGN) : 0;
    if (is_main) {
        c_cursor = tls_at + NXABI_TLS_SIZE;
    }
    const std::uint64_t c_memsz = c_cursor - c_at;

    // ---- stubs for real ----------------------------------------------------------
    StubTargets targets{info_at, rw_at, got_at, H};
    Code stubs = emit_stubs(b_at, targets, sites, trap_names.size(), is_main, stub_plan);
    if (stubs.size_bytes() != stub_size) {
        fail("internal error: stub size changed between passes");
    }

    // ---- patched image -----------------------------------------------------------
    Bytes image = m.image;
    for (std::size_t i = 0; i < sites.size(); ++i) {
        const PatchSite& s = sites[i];
        std::uint32_t word = 0;
        if (s.kind == PatchKind::OrderedExclusive) {
            word = a64::make_ordered_exclusive(s.insn);
        } else if (s.kind == PatchKind::Nop) {
            word = a64::kNop;
        } else {
            word = a64::b(H + s.offset, stub_plan.site_stub[i]);
        }
        store_le<std::uint32_t>(image.data() + s.offset, word);
    }

    // ---- sections ------------------------------------------------------------------
    std::vector<SectionSpec> sections;
    sections.push_back({});
    auto add_section = [&](SectionSpec s) {
        sections.push_back(std::move(s));
        return static_cast<std::uint32_t>(sections.size() - 1);
    };
    if (is_main) {
        add_section({".interp", elf::SHT_PROGBITS, elf::SHF_ALLOC, interp_at, interp_at,
                     opt.interpreter.size() + 1, 0, 0, 1, 0});
    }
    std::uint32_t sec_dynsym = add_section({".dynsym", elf::SHT_DYNSYM, elf::SHF_ALLOC, dynsym_at, dynsym_at,
                                            nsyms * sizeof(elf::Sym), 0, 1, 8, sizeof(elf::Sym)});
    std::uint32_t sec_dynstr = add_section({".dynstr", elf::SHT_STRTAB, elf::SHF_ALLOC, dynstr_at, dynstr_at,
                                            dynstr_bytes.size(), 0, 0, 1, 0});
    sections[sec_dynsym].link = sec_dynstr;
    add_section({".hash", elf::SHT_HASH, elf::SHF_ALLOC, hash_at, hash_at, (2 + nbucket + nsyms) * 4, sec_dynsym,
                 0, 8, 4});
    if (n_dyn) {
        add_section({".rela.dyn", elf::SHT_RELA, elf::SHF_ALLOC, rela_at, rela_at, n_dyn * sizeof(elf::Rela),
                     sec_dynsym, 0, 8, sizeof(elf::Rela)});
    }
    if (n_plt) {
        add_section({".rela.plt", elf::SHT_RELA, elf::SHF_ALLOC, plt_at, plt_at, n_plt * sizeof(elf::Rela),
                     sec_dynsym, 0, 8, sizeof(elf::Rela)});
    }
    add_section({".nx.modinfo", elf::SHT_PROGBITS, elf::SHF_ALLOC, info_at, info_at, sizeof(nxabi_module_info), 0,
                 0, 8, 0});
    if (!trap_blob.empty()) {
        add_section({".nx.trapnames", elf::SHT_PROGBITS, elf::SHF_ALLOC, traps_at, traps_at, trap_blob.size(), 0,
                     0, 8, 0});
    }
    if (has_npdm) {
        add_section({".nx.npdm", elf::SHT_PROGBITS, elf::SHF_ALLOC, npdm_at, npdm_at, opt.npdm_raw.size(), 0, 0,
                     8, 0});
    }
    if (stub_size) {
        add_section({".nx.stubs", elf::SHT_PROGBITS, elf::SHF_ALLOC | elf::SHF_EXECINSTR, b_at, b_at, stub_size, 0,
                     0, 4, 0});
    }
    std::uint32_t sec_text = add_section({".text", elf::SHT_PROGBITS, elf::SHF_ALLOC | elf::SHF_EXECINSTR,
                                          H + m.text.offset, H + m.text.offset, m.text.size, 0, 0, 4, 0});
    std::uint32_t sec_ro = has_ro ? add_section({".rodata", elf::SHT_PROGBITS, elf::SHF_ALLOC, H + m.ro.offset,
                                                 H + m.ro.offset, m.ro.size, 0, 0, 8, 0})
                                  : 0;
    std::uint32_t sec_data = add_section({".data", elf::SHT_PROGBITS, elf::SHF_ALLOC | elf::SHF_WRITE,
                                          H + m.data.offset, H + m.data.offset, m.data.size, 0, 0, 8, 0});
    std::uint32_t sec_bss = add_section({".bss", elf::SHT_NOBITS, elf::SHF_ALLOC | elf::SHF_WRITE,
                                         H + m.bss.offset, H + m.bss.offset, m.image.size() - m.bss.offset, 0, 0,
                                         8, 0});
    add_section({".dynamic", elf::SHT_DYNAMIC, elf::SHF_ALLOC | elf::SHF_WRITE, dynamic_at, dynamic_at,
                 ndyn * sizeof(elf::Dyn), sec_dynstr, 0, 8, sizeof(elf::Dyn)});
    if (is_main) {
        add_section({".nx.got", elf::SHT_PROGBITS, elf::SHF_ALLOC | elf::SHF_WRITE, got_at, got_at, 8, 0, 0, 8, 8});
    }
    add_section({".nx.rw", elf::SHT_PROGBITS, elf::SHF_ALLOC | elf::SHF_WRITE, rw_at, rw_at,
                 sizeof(nxabi_module_rw), 0, 0, 64, 0});
    if (is_main) {
        add_section({".tbss", elf::SHT_NOBITS, elf::SHF_ALLOC | elf::SHF_WRITE | elf::SHF_TLS, tls_at, tls_at,
                     NXABI_TLS_SIZE, 0, 0, NXABI_TLS_ALIGN, 0});
    }

    auto section_for = [&](std::uint64_t image_offset) -> std::uint16_t {
        if (m.text.contains(image_offset)) return static_cast<std::uint16_t>(sec_text);
        if (has_ro && m.ro.contains(image_offset)) return static_cast<std::uint16_t>(sec_ro);
        if (m.data.contains(image_offset)) return static_cast<std::uint16_t>(sec_data);
        if (image_offset >= m.bss.offset && image_offset <= m.image.size()) return static_cast<std::uint16_t>(sec_bss);
        if (image_offset == m.text.end()) return static_cast<std::uint16_t>(sec_text);
        if (image_offset == m.data.end()) return static_cast<std::uint16_t>(sec_data);
        return static_cast<std::uint16_t>(sec_text);
    };

    // ---- dynamic symbols -----------------------------------------------------------
    std::vector<elf::Sym> dynsym(nsyms);
    for (std::size_t i = 1; i < m.symbols.size(); ++i) {
        const Symbol& s = m.symbols[i];
        elf::Sym& o = dynsym[i];
        o.name = sym_names[i];
        o.info = s.info;
        o.other = s.other;
        o.size = s.size;
        if (!s.defined()) {
            o.shndx = elf::SHN_UNDEF;
            o.value = 0;
        } else if (s.absolute() || s.type() == elf::STT_TLS) {
            o.shndx = s.shndx;
            o.value = s.value;
        } else {
            o.shndx = section_for(s.value);
            o.value = H + s.value;
        }
    }
    if (is_main) {
        dynsym[entry_sym] = {entry_name, elf::st_info(elf::STB_GLOBAL, elf::STT_FUNC), 0, elf::SHN_UNDEF, 0, 0};
    }

    // ---- hash ------------------------------------------------------------------------
    std::vector<std::uint32_t> buckets(nbucket, 0), chains(nsyms, 0);
    for (std::size_t i = 1; i < nsyms; ++i) {
        if (dynsym[i].name == 0) {
            continue;
        }
        const char* name = reinterpret_cast<const char*>(dynstr_bytes.data() + dynsym[i].name);
        std::uint32_t h = elf::sysv_hash(name) % nbucket;
        chains[i] = buckets[h];
        buckets[h] = static_cast<std::uint32_t>(i);
    }

    // ---- relocations -------------------------------------------------------------------
    std::vector<elf::Rela> rela_dyn, rela_plt;
    if (is_main) {
        rela_dyn.push_back({got_at, elf::r_info(entry_sym, elf::R_AARCH64_GLOB_DAT), 0});
    }
    for (std::size_t i = 0; i < m.relocs.size(); ++i) {
        const Reloc& r = m.relocs[i];
        const std::uint64_t where = H + r.offset;
        switch (plan[i]) {
        case RelocPlan::Drop: break;
        case RelocPlan::Relative:
            rela_dyn.push_back({where, elf::r_info(0, elf::R_AARCH64_RELATIVE), r.addend + static_cast<std::int64_t>(H)});
            break;
        case RelocPlan::Symbolic: rela_dyn.push_back({where, elf::r_info(r.sym, r.type), r.addend}); break;
        case RelocPlan::Plt: rela_plt.push_back({where, elf::r_info(r.sym, r.type), r.addend}); break;
        case RelocPlan::Trap: {
            std::uint64_t stub = stub_plan.trap_stub[static_cast<std::size_t>(trap_of[r.sym])];
            std::int64_t addend = r.type == elf::R_AARCH64_ABS64 ? r.addend : 0;
            rela_dyn.push_back({where, elf::r_info(0, elf::R_AARCH64_RELATIVE), static_cast<std::int64_t>(stub) + addend});
            break;
        }
        }
    }

    // ---- dynamic section -------------------------------------------------------------
    std::vector<elf::Dyn> dyn;
    for (auto n : needed_names) dyn.push_back({elf::DT_NEEDED, n});
    if (opt.kind == OutputKind::Shared) dyn.push_back({elf::DT_SONAME, soname_name});
    if (!opt.runpath.empty()) dyn.push_back({elf::DT_RUNPATH, runpath_name});
    dyn.push_back({elf::DT_HASH, hash_at});
    dyn.push_back({elf::DT_STRTAB, dynstr_at});
    dyn.push_back({elf::DT_SYMTAB, dynsym_at});
    dyn.push_back({elf::DT_STRSZ, dynstr_bytes.size()});
    dyn.push_back({elf::DT_SYMENT, sizeof(elf::Sym)});
    if (n_dyn) {
        dyn.push_back({elf::DT_RELA, rela_at});
        dyn.push_back({elf::DT_RELASZ, n_dyn * sizeof(elf::Rela)});
        dyn.push_back({elf::DT_RELAENT, sizeof(elf::Rela)});
    }
    if (n_plt) {
        dyn.push_back({elf::DT_JMPREL, plt_at});
        dyn.push_back({elf::DT_PLTRELSZ, n_plt * sizeof(elf::Rela)});
        dyn.push_back({elf::DT_PLTREL, static_cast<std::uint64_t>(elf::DT_RELA)});
        if (m.pltgot) dyn.push_back({elf::DT_PLTGOT, H + m.pltgot});
    }
    dyn.push_back({elf::DT_BIND_NOW, 0});
    dyn.push_back({elf::DT_FLAGS, elf::DF_BIND_NOW | (textrel ? elf::DF_TEXTREL : 0)});
    if (textrel) dyn.push_back({elf::DT_TEXTREL, 0});
    dyn.push_back({elf::DT_FLAGS_1, elf::DF_1_NOW | (is_main ? elf::DF_1_PIE : 0)});
    if (is_main) dyn.push_back({elf::DT_DEBUG, 0});
    dyn.push_back({elf::DT_NULL, 0});
    if (dyn.size() != ndyn || rela_dyn.size() != n_dyn || rela_plt.size() != n_plt) {
        fail("internal error: layout counts changed while writing");
    }

    // ---- module info -------------------------------------------------------------------
    nxabi_module_info info{};
    info.magic = NXABI_MODULE_INFO_MAGIC;
    info.abi_version = NXABI_VERSION;
    info.flags = (is_main ? NXABI_MODULE_MAIN : 0) | (sites.empty() ? 0 : NXABI_MODULE_PATCHED) |
                 (opt.patch && opt.scan.ordered_exclusives ? NXABI_MODULE_ORDERED_EXCLUSIVES : 0);
    info.load_order = opt.load_order;
    std::memcpy(info.name, opt.module_name.data(), std::min(opt.module_name.size(), sizeof(info.name) - 1));
    std::memcpy(info.module_id, m.module_id.data(), sizeof(info.module_id));
    info.image_offset = H;
    info.image_size = m.image.size();
    info.text_offset = H + m.text.offset;
    info.text_size = m.text.size;
    info.ro_offset = has_ro ? H + m.ro.offset : 0;
    info.ro_size = m.ro.size;
    info.data_offset = H + m.data.offset;
    info.data_size = m.data.size;
    info.bss_offset = H + m.bss.offset;
    info.bss_size = m.bss.size;
    info.mod0_offset = H + m.mod0.offset;
    if (has_eh) {
        info.eh_frame_hdr_offset = H + m.mod0.eh_frame_hdr_start;
        info.eh_frame_hdr_size = m.mod0.eh_frame_hdr_end - m.mod0.eh_frame_hdr_start;
    }
    info.init_offset = m.init ? H + m.init : 0;
    info.fini_offset = m.fini ? H + m.fini : 0;
    info.init_array_offset = m.init_array ? H + m.init_array : 0;
    info.init_array_count = m.init_array_size / 8;
    info.fini_array_offset = m.fini_array ? H + m.fini_array : 0;
    info.fini_array_count = m.fini_array_size / 8;
    if (const Symbol* s = m.find_export("nnMain")) info.nnmain_offset = H + s->value;
    if (const Symbol* s = m.find_export("nninitStartup")) info.nninit_startup_offset = H + s->value;
    info.rw_offset = rw_at;
    info.trap_names_offset = traps_at;
    info.trap_count = static_cast<std::uint32_t>(trap_names.size());
    info.patch_count = static_cast<std::uint32_t>(sites.size());
    info.npdm_offset = npdm_at;
    info.npdm_size = has_npdm ? opt.npdm_raw.size() : 0;
    info.address_space = opt.npdm ? opt.npdm->address_space : NXABI_AS_UNKNOWN;
    info.main_thread_priority = opt.npdm ? opt.npdm->main_thread_priority : 0;
    info.main_thread_core = opt.npdm ? opt.npdm->main_thread_core : 0;
    info.main_thread_stack_size = opt.npdm ? opt.npdm->main_thread_stack_size : 0;
    info.program_id = opt.npdm && opt.npdm->program_id ? *opt.npdm->program_id : 0;

    // ---- static symbol table for debuggers (original names) --------------------------
    StringTable strtab;
    std::vector<elf::Sym> symtab(1);
    auto local = [&](const char* name, std::uint64_t at, std::uint32_t section) {
        if (at) symtab.push_back({strtab.add(name), elf::st_info(elf::STB_LOCAL, elf::STT_FUNC), 0,
                                  static_cast<std::uint16_t>(section), at, 0});
    };
    std::uint32_t sec_stubs = 0;
    for (std::uint32_t i = 0; i < sections.size(); ++i) {
        if (sections[i].name == ".nx.stubs") sec_stubs = i;
    }
    local("nx.entry", stub_plan.entry, sec_stubs);
    local("nx.svc_common", stub_plan.svc_common, sec_stubs);
    local("nx.tick_common", stub_plan.tick_common, sec_stubs);
    local("nx.trap_common", stub_plan.trap_common, sec_stubs);
    std::uint32_t first_global = static_cast<std::uint32_t>(symtab.size());
    for (std::size_t i = 1; i < m.symbols.size(); ++i) {
        const Symbol& s = m.symbols[i];
        if (!s.defined() || s.name.empty() || s.bind() == elf::STB_LOCAL) continue;
        elf::Sym o = dynsym[i];
        o.name = strtab.add(s.name);
        symtab.push_back(o);
    }

    // ---- program headers ------------------------------------------------------------
    std::vector<elf::Phdr> ph;
    const std::uint64_t phdr_size = nphdr * sizeof(elf::Phdr);
    ph.push_back({elf::PT_PHDR, elf::PF_R, sizeof(elf::Ehdr), sizeof(elf::Ehdr), sizeof(elf::Ehdr), phdr_size,
                  phdr_size, 8});
    if (is_main) {
        std::uint64_t n = opt.interpreter.size() + 1;
        ph.push_back({elf::PT_INTERP, elf::PF_R, interp_at, interp_at, interp_at, n, n, 1});
    }
    ph.push_back({elf::PT_LOAD, elf::PF_R, 0, 0, 0, a_end, a_end, kSegAlign});
    ph.push_back({elf::PT_LOAD, elf::PF_R | elf::PF_X, b_at, b_at, b_at, std::max<std::uint64_t>(stub_size, 4),
                  std::max<std::uint64_t>(stub_size, 4), kSegAlign});
    ph.push_back({elf::PT_LOAD, elf::PF_R | elf::PF_X, H + m.text.offset, H + m.text.offset, H + m.text.offset,
                  m.text.size, m.text.size, kSegAlign});
    if (has_ro) {
        ph.push_back({elf::PT_LOAD, elf::PF_R, H + m.ro.offset, H + m.ro.offset, H + m.ro.offset, m.ro.size,
                      m.ro.size, kSegAlign});
    }
    ph.push_back({elf::PT_LOAD, elf::PF_R | elf::PF_W, H + m.data.offset, H + m.data.offset, H + m.data.offset,
                  m.data.size, m.image.size() - m.data.offset, kSegAlign});
    ph.push_back({elf::PT_LOAD, elf::PF_R | elf::PF_W, c_at, c_at, c_at, c_filesz, c_memsz, kSegAlign});
    ph.push_back({elf::PT_DYNAMIC, elf::PF_R | elf::PF_W, dynamic_at, dynamic_at, dynamic_at,
                  ndyn * sizeof(elf::Dyn), ndyn * sizeof(elf::Dyn), 8});
    if (is_main) {
        ph.push_back({elf::PT_TLS, elf::PF_R, tls_at, tls_at, tls_at, 0, NXABI_TLS_SIZE, NXABI_TLS_ALIGN});
    }
    if (has_eh) {
        std::uint64_t at = H + m.mod0.eh_frame_hdr_start;
        std::uint64_t n = m.mod0.eh_frame_hdr_end - m.mod0.eh_frame_hdr_start;
        ph.push_back({elf::PT_GNU_EH_FRAME, elf::PF_R, at, at, at, n, n, 4});
    }
    ph.push_back({elf::PT_GNU_STACK, elf::PF_R | elf::PF_W, 0, 0, 0, 0, 0, 16});
    ph.push_back({NXABI_PT_MODULE_INFO, elf::PF_R, info_at, info_at, info_at, sizeof(info), sizeof(info), 8});
    if (ph.size() != nphdr) {
        fail("internal error: program header count mismatch");
    }

    // ---- serialize ---------------------------------------------------------------------
    Bytes out(c_at + c_filesz, 0);
    auto put = [&](std::uint64_t at, const void* src, std::size_t n) {
        if (n) std::memcpy(out.data() + at, src, n);
    };
    std::memcpy(out.data() + sizeof(elf::Ehdr), ph.data(), phdr_size);
    if (is_main) put(interp_at, opt.interpreter.c_str(), opt.interpreter.size() + 1);
    put(dynsym_at, dynsym.data(), dynsym.size() * sizeof(elf::Sym));
    put(dynstr_at, dynstr_bytes.data(), dynstr_bytes.size());
    {
        std::vector<std::uint32_t> hash;
        hash.push_back(static_cast<std::uint32_t>(nbucket));
        hash.push_back(static_cast<std::uint32_t>(nsyms));
        hash.insert(hash.end(), buckets.begin(), buckets.end());
        hash.insert(hash.end(), chains.begin(), chains.end());
        put(hash_at, hash.data(), hash.size() * 4);
    }
    put(rela_at, rela_dyn.data(), rela_dyn.size() * sizeof(elf::Rela));
    put(plt_at, rela_plt.data(), rela_plt.size() * sizeof(elf::Rela));
    put(info_at, &info, sizeof(info));
    if (!trap_blob.empty()) put(traps_at, trap_blob.data(), trap_blob.size());
    if (has_npdm) put(npdm_at, opt.npdm_raw.data(), opt.npdm_raw.size());
    put(b_at, stubs.words().data(), stubs.size_bytes());
    put(H, image.data(), m.data.end());
    put(dynamic_at, dyn.data(), dyn.size() * sizeof(elf::Dyn));

    // non-loaded trailer: .symtab, .strtab, .shstrtab, section headers
    Writer tail;
    tail.bytes() = std::move(out);
    tail.align(8);
    std::uint64_t symtab_at = tail.size();
    for (const auto& s : symtab) tail.put(s);
    std::uint64_t strtab_at = tail.size();
    tail.put_bytes(strtab.bytes());
    std::uint32_t sec_symtab = add_section({".symtab", elf::SHT_SYMTAB, 0, 0, symtab_at,
                                            symtab.size() * sizeof(elf::Sym), 0, first_global, 8, sizeof(elf::Sym)});
    std::uint32_t sec_strtab = add_section({".strtab", elf::SHT_STRTAB, 0, 0, strtab_at, strtab.bytes().size(), 0,
                                            0, 1, 0});
    sections[sec_symtab].link = sec_strtab;
    std::uint32_t sec_shstrtab = add_section({".shstrtab", elf::SHT_STRTAB, 0, 0, 0, 0, 0, 0, 1, 0});
    StringTable shstr;
    std::vector<std::uint32_t> name_offsets;
    for (const auto& s : sections) name_offsets.push_back(shstr.add(s.name));
    std::uint64_t shstrtab_at = tail.size();
    tail.put_bytes(shstr.bytes());
    sections[sec_shstrtab].offset = shstrtab_at;
    sections[sec_shstrtab].size = shstr.bytes().size();
    tail.align(8);
    std::uint64_t shoff = tail.size();
    for (std::size_t i = 0; i < sections.size(); ++i) {
        const auto& s = sections[i];
        elf::Shdr sh{name_offsets[i], s.type, s.flags, s.addr, s.offset, s.size, s.link, s.info, s.align, s.entsize};
        if (i == 0) sh = elf::Shdr{};
        tail.put(sh);
    }

    elf::Ehdr eh{};
    const unsigned char ident[16] = {0x7f, 'E', 'L', 'F', 2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    std::memcpy(eh.ident, ident, 16);
    eh.type = elf::ET_DYN;
    eh.machine = elf::EM_AARCH64;
    eh.version = 1;
    eh.entry = is_main ? stub_plan.entry : 0;
    eh.phoff = sizeof(elf::Ehdr);
    eh.shoff = shoff;
    eh.ehsize = sizeof(elf::Ehdr);
    eh.phentsize = sizeof(elf::Phdr);
    eh.phnum = static_cast<std::uint16_t>(ph.size());
    eh.shentsize = sizeof(elf::Shdr);
    eh.shnum = static_cast<std::uint16_t>(sections.size());
    eh.shstrndx = static_cast<std::uint16_t>(sec_shstrtab);
    std::memcpy(tail.bytes().data(), &eh, sizeof(eh));

    result.elf = std::move(tail.bytes());
    return result;
}

} // namespace nx
