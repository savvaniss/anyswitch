// SPDX-License-Identifier: MIT
// Finds the instructions that cannot run unchanged in a host process and
// decides how each is handled. See docs/dev/ARCHITECTURE.md, "Patch classes".
#pragma once

#include "nx/format/module.hpp"

#include <vector>

namespace nx {

enum class PatchKind : std::uint8_t {
    Svc,              // svc #imm                -> stub calling the runtime SVC handler
    ReadTlr,          // mrs xN, tpidrro_el0     -> stub loading the guest TLR pointer
    ReadGuestTp,      // mrs xN, tpidr_el0       -> stub loading the guest thread pointer
    WriteGuestTp,     // msr tpidr_el0, xN       -> stub storing the guest thread pointer
    ReadTick,         // mrs xN, cntpct/cntvct   -> stub rescaling the host counter to 19.2 MHz
    ReadTickFreq,     // mrs xN, cntfrq_el0      -> stub returning 19200000
    OrderedExclusive, // ldxr/stxr family        -> rewritten in place to ldaxr/stlxr
    Nop,              // reads into xzr          -> rewritten in place to nop
};

struct PatchSite {
    std::uint64_t offset = 0; // image offset of the instruction
    std::uint32_t insn = 0;
    PatchKind kind = PatchKind::Nop;
    std::uint32_t reg = 0;
    std::uint32_t svc = 0;
};

struct PatchScanOptions {
    bool ordered_exclusives = true;
};

std::vector<PatchSite> find_patch_sites(const Module& module, const PatchScanOptions& options);
const char* patch_kind_name(PatchKind kind);
bool patch_needs_stub(const PatchSite& site);

} // namespace nx
