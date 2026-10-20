// SPDX-License-Identifier: MIT
#include "nx/relinker/census.hpp"

#include "nx/relinker/a64.hpp"
#include "nx/relinker/json.hpp"

#include <format>

namespace nx {
namespace {

std::string sysreg_name(std::uint32_t reg) {
    switch (reg) {
    case a64::kTpidrEl0: return "tpidr_el0";
    case a64::kTpidrroEl0: return "tpidrro_el0";
    case a64::kCntfrqEl0: return "cntfrq_el0";
    case a64::kCntpctEl0: return "cntpct_el0";
    case a64::kCntvctEl0: return "cntvct_el0";
    case a64::kDczidEl0: return "dczid_el0";
    case a64::kCtrEl0: return "ctr_el0";
    case a64::kNzcv: return "nzcv";
    case a64::sysreg(3, 3, 4, 4, 0): return "fpcr";
    case a64::sysreg(3, 3, 4, 4, 1): return "fpsr";
    default: break;
    }
    return std::format("s{}_{}_c{}_c{}_{}", 2 + ((reg >> 19) & 1), (reg >> 16) & 7, (reg >> 12) & 15,
                       (reg >> 8) & 15, (reg >> 5) & 7);
}

std::string cache_op_name(const a64::SysOp& op) {
    if (op.crn == 7) {
        if (op.op1 == 3 && op.crm == 4 && op.op2 == 1) return "dc_zva";
        if (op.op1 == 3 && op.crm == 10 && op.op2 == 1) return "dc_cvac";
        if (op.op1 == 3 && op.crm == 11 && op.op2 == 1) return "dc_cvau";
        if (op.op1 == 3 && op.crm == 14 && op.op2 == 1) return "dc_civac";
        if (op.op1 == 3 && op.crm == 5 && op.op2 == 1) return "ic_ivau";
        if (op.op1 == 0 && op.crm == 5 && op.op2 == 0) return "ic_iallu (EL1 only)";
        if (op.op1 == 0 && op.crm == 1 && op.op2 == 0) return "ic_ialluis (EL1 only)";
        if (op.op1 == 0 && op.crm == 6) return "dc_ivac/isw (EL1 only)";
    }
    return std::format("sys_{}_c{}_c{}_{}", op.op1, op.crn, op.crm, op.op2);
}

} // namespace

Census run_census(const Module& m) {
    Census c;
    c.module = m.name;
    Reader img(m.image, "image");
    for (std::uint64_t at = m.text.offset; at + 4 <= m.text.end(); at += 4) {
        std::uint32_t insn = img.u32(at);
        ++c.text_words;
        switch (a64::decode_exception(insn)) {
        case a64::ExceptionKind::Svc: ++c.svc_by_number[(insn >> 5) & 0xFFFF]; continue;
        case a64::ExceptionKind::Hvc: ++c.exceptions["hvc"]; continue;
        case a64::ExceptionKind::Smc: ++c.exceptions["smc"]; continue;
        case a64::ExceptionKind::Brk: ++c.exceptions["brk"]; continue;
        case a64::ExceptionKind::Hlt: ++c.exceptions["hlt"]; continue;
        case a64::ExceptionKind::None: break;
        }
        if (auto mv = a64::decode_sysreg_move(insn)) {
            auto& bucket = mv->is_read ? c.sysreg_reads : c.sysreg_writes;
            ++bucket[sysreg_name(mv->reg)];
            continue;
        }
        if (auto op = a64::decode_sys(insn)) {
            ++c.cache_ops[cache_op_name(*op)];
            continue;
        }
        if (a64::is_exclusive(insn)) {
            if (a64::is_ordered_exclusive(insn)) {
                ++c.exclusive_ordered;
            } else {
                ++c.exclusive_unordered;
            }
        }
    }
    for (const auto& r : m.relocs) {
        ++c.relocations[elf::reloc_name(r.type)];
    }
    c.imports = m.import_count();
    c.exports = m.export_count();
    return c;
}

std::string census_to_json(const Census& c) {
    json::Builder j;
    j.begin_object();
    j.key("module").value(c.module);
    j.key("text_words").value(c.text_words);
    j.key("svc").begin_object();
    for (const auto& [num, count] : c.svc_by_number) {
        j.key(std::format("{:#04x}", num)).value(count);
    }
    j.end_object();
    auto map = [&](const char* name, const std::map<std::string, std::uint64_t>& values) {
        j.key(name).begin_object();
        for (const auto& [k, v] : values) {
            j.key(k).value(v);
        }
        j.end_object();
    };
    map("sysreg_reads", c.sysreg_reads);
    map("sysreg_writes", c.sysreg_writes);
    map("cache_ops", c.cache_ops);
    map("exceptions", c.exceptions);
    j.key("exclusive_unordered").value(c.exclusive_unordered);
    j.key("exclusive_ordered").value(c.exclusive_ordered);
    map("relocations", c.relocations);
    j.key("imports").value(c.imports);
    j.key("exports").value(c.exports);
    j.end_object();
    return j.str();
}

std::string census_to_text(const Census& c) {
    std::string out = std::format("{}: {} instruction words, {} imports, {} exports\n", c.module, c.text_words,
                                  c.imports, c.exports);
    auto line = [&](const std::string& what, std::uint64_t n) { out += std::format("  {:<28} {}\n", what, n); };
    for (const auto& [num, count] : c.svc_by_number) {
        line(std::format("svc {:#04x}", num), count);
    }
    for (const auto& [k, v] : c.sysreg_reads) {
        line("mrs " + k, v);
    }
    for (const auto& [k, v] : c.sysreg_writes) {
        line("msr " + k, v);
    }
    for (const auto& [k, v] : c.cache_ops) {
        line(k, v);
    }
    for (const auto& [k, v] : c.exceptions) {
        line(k, v);
    }
    line("exclusive (unordered)", c.exclusive_unordered);
    line("exclusive (ordered)", c.exclusive_ordered);
    for (const auto& [k, v] : c.relocations) {
        line(k, v);
    }
    return out;
}

} // namespace nx
