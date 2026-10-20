// SPDX-License-Identifier: MIT
#include "nx/relinker/patcher.hpp"

#include "nx/relinker/a64.hpp"

namespace nx {

std::vector<PatchSite> find_patch_sites(const Module& m, const PatchScanOptions& options) {
    std::vector<PatchSite> sites;
    Reader img(m.image, "image");
    std::uint64_t begin = align_up(m.text.offset, 4);
    std::uint64_t end = m.text.end() & ~std::uint64_t(3);
    for (std::uint64_t at = begin; at < end; at += 4) {
        std::uint32_t insn = img.u32(at);
        PatchSite site;
        site.offset = at;
        site.insn = insn;
        if (auto imm = a64::decode_svc(insn)) {
            site.kind = PatchKind::Svc;
            site.svc = *imm;
            sites.push_back(site);
            continue;
        }
        if (auto mv = a64::decode_sysreg_move(insn)) {
            site.reg = mv->rt;
            bool matched = true;
            if (mv->is_read) {
                switch (mv->reg) {
                case a64::kTpidrroEl0: site.kind = PatchKind::ReadTlr; break;
                case a64::kTpidrEl0: site.kind = PatchKind::ReadGuestTp; break;
                case a64::kCntpctEl0:
                case a64::kCntvctEl0: site.kind = PatchKind::ReadTick; break;
                case a64::kCntfrqEl0: site.kind = PatchKind::ReadTickFreq; break;
                default: matched = false; break;
                }
                if (matched && mv->rt == a64::kZr) {
                    site.kind = PatchKind::Nop;
                }
            } else {
                matched = mv->reg == a64::kTpidrEl0;
                site.kind = PatchKind::WriteGuestTp;
            }
            if (matched) {
                sites.push_back(site);
            }
            continue;
        }
        if (options.ordered_exclusives && a64::is_exclusive(insn) && !a64::is_ordered_exclusive(insn)) {
            site.kind = PatchKind::OrderedExclusive;
            sites.push_back(site);
        }
    }
    return sites;
}

const char* patch_kind_name(PatchKind kind) {
    switch (kind) {
    case PatchKind::Svc: return "svc";
    case PatchKind::ReadTlr: return "mrs_tpidrro_el0";
    case PatchKind::ReadGuestTp: return "mrs_tpidr_el0";
    case PatchKind::WriteGuestTp: return "msr_tpidr_el0";
    case PatchKind::ReadTick: return "mrs_counter";
    case PatchKind::ReadTickFreq: return "mrs_cntfrq_el0";
    case PatchKind::OrderedExclusive: return "exclusive_to_ordered";
    case PatchKind::Nop: return "nop";
    }
    return "unknown";
}

bool patch_needs_stub(const PatchSite& site) {
    return site.kind != PatchKind::OrderedExclusive && site.kind != PatchKind::Nop;
}

} // namespace nx
