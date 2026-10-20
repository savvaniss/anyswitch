// SPDX-License-Identifier: MIT
// AArch64 instruction encoders and the few decoders the relinker needs.
// Encodings follow the Arm Architecture Reference Manual (A64 base
// instructions); every encoder is covered by a known-value unit test.
#pragma once

#include "nx/common/error.hpp"

#include <cstdint>
#include <optional>

namespace nx::a64 {

constexpr std::uint32_t kNop = 0xD503201F;
constexpr std::uint32_t kRet = 0xD65F03C0;
constexpr std::uint32_t kSp = 31;
constexpr std::uint32_t kZr = 31;

// System register operand, already shifted into instruction bits [20:5].
constexpr std::uint32_t sysreg(std::uint32_t op0, std::uint32_t op1, std::uint32_t crn, std::uint32_t crm,
                               std::uint32_t op2) {
    return (op0 << 19) | (op1 << 16) | (crn << 12) | (crm << 8) | (op2 << 5);
}

constexpr std::uint32_t kTpidrEl0 = sysreg(3, 3, 13, 0, 2);
constexpr std::uint32_t kTpidrroEl0 = sysreg(3, 3, 13, 0, 3);
constexpr std::uint32_t kCntfrqEl0 = sysreg(3, 3, 14, 0, 0);
constexpr std::uint32_t kCntpctEl0 = sysreg(3, 3, 14, 0, 1);
constexpr std::uint32_t kCntvctEl0 = sysreg(3, 3, 14, 0, 2);
constexpr std::uint32_t kDczidEl0 = sysreg(3, 3, 0, 0, 7);
constexpr std::uint32_t kCtrEl0 = sysreg(3, 3, 0, 0, 1);
constexpr std::uint32_t kNzcv = sysreg(3, 3, 4, 2, 0);
constexpr std::uint32_t kSysregMask = 0x001FFFE0;

// ---- encoders --------------------------------------------------------------

inline std::uint32_t branch_imm(std::uint32_t opcode, std::uint64_t from, std::uint64_t to) {
    std::int64_t delta = static_cast<std::int64_t>(to) - static_cast<std::int64_t>(from);
    if (delta % 4 != 0 || delta < -(std::int64_t(1) << 27) || delta >= (std::int64_t(1) << 27)) {
        fail("branch from {:#x} to {:#x} is out of the +/-128 MiB range", from, to);
    }
    return opcode | (static_cast<std::uint32_t>(delta >> 2) & 0x03FFFFFF);
}

inline std::uint32_t b(std::uint64_t from, std::uint64_t to) { return branch_imm(0x14000000, from, to); }
inline std::uint32_t bl(std::uint64_t from, std::uint64_t to) { return branch_imm(0x94000000, from, to); }
constexpr std::uint32_t br(std::uint32_t rn) { return 0xD61F0000 | (rn << 5); }
constexpr std::uint32_t blr(std::uint32_t rn) { return 0xD63F0000 | (rn << 5); }

inline std::uint32_t adrp(std::uint32_t rd, std::uint64_t pc, std::uint64_t target) {
    std::int64_t pages = (static_cast<std::int64_t>(target & ~0xFFFull) - static_cast<std::int64_t>(pc & ~0xFFFull)) >> 12;
    if (pages < -(std::int64_t(1) << 20) || pages >= (std::int64_t(1) << 20)) {
        fail("adrp from {:#x} to {:#x} is out of the +/-4 GiB range", pc, target);
    }
    auto imm = static_cast<std::uint32_t>(pages) & 0x1FFFFF;
    return 0x90000000 | ((imm & 3) << 29) | ((imm >> 2) << 5) | rd;
}

constexpr std::uint32_t add_imm(std::uint32_t rd, std::uint32_t rn, std::uint32_t imm12) {
    return 0x91000000 | ((imm12 & 0xFFF) << 10) | (rn << 5) | rd;
}
constexpr std::uint32_t sub_imm(std::uint32_t rd, std::uint32_t rn, std::uint32_t imm12) {
    return 0xD1000000 | ((imm12 & 0xFFF) << 10) | (rn << 5) | rd;
}
constexpr std::uint32_t mov_from_sp(std::uint32_t rd) { return add_imm(rd, kSp, 0); }
constexpr std::uint32_t mov_reg(std::uint32_t rd, std::uint32_t rm) { return 0xAA0003E0 | (rm << 16) | rd; }
constexpr std::uint32_t mov_reg_w(std::uint32_t rd, std::uint32_t rm) { return 0x2A0003E0 | (rm << 16) | rd; }
constexpr std::uint32_t add_reg(std::uint32_t rd, std::uint32_t rn, std::uint32_t rm) {
    return 0x8B000000 | (rm << 16) | (rn << 5) | rd;
}
constexpr std::uint32_t mul(std::uint32_t rd, std::uint32_t rn, std::uint32_t rm) {
    return 0x9B007C00 | (rm << 16) | (rn << 5) | rd;
}
constexpr std::uint32_t umulh(std::uint32_t rd, std::uint32_t rn, std::uint32_t rm) {
    return 0x9BC07C00 | (rm << 16) | (rn << 5) | rd;
}
constexpr std::uint32_t movz_x(std::uint32_t rd, std::uint32_t imm16, std::uint32_t shift) {
    return 0xD2800000 | ((shift / 16) << 21) | ((imm16 & 0xFFFF) << 5) | rd;
}
constexpr std::uint32_t movk_x(std::uint32_t rd, std::uint32_t imm16, std::uint32_t shift) {
    return 0xF2800000 | ((shift / 16) << 21) | ((imm16 & 0xFFFF) << 5) | rd;
}
constexpr std::uint32_t movz_w(std::uint32_t rd, std::uint32_t imm16, std::uint32_t shift) {
    return 0x52800000 | ((shift / 16) << 21) | ((imm16 & 0xFFFF) << 5) | rd;
}
constexpr std::uint32_t movk_w(std::uint32_t rd, std::uint32_t imm16, std::uint32_t shift) {
    return 0x72800000 | ((shift / 16) << 21) | ((imm16 & 0xFFFF) << 5) | rd;
}

inline std::uint32_t ldr_x(std::uint32_t rt, std::uint32_t rn, std::uint32_t byte_offset) {
    if (byte_offset % 8 != 0 || byte_offset / 8 > 0xFFF) {
        fail("ldr offset {:#x} is not encodable", byte_offset);
    }
    return 0xF9400000 | ((byte_offset / 8) << 10) | (rn << 5) | rt;
}
inline std::uint32_t str_x(std::uint32_t rt, std::uint32_t rn, std::uint32_t byte_offset) {
    if (byte_offset % 8 != 0 || byte_offset / 8 > 0xFFF) {
        fail("str offset {:#x} is not encodable", byte_offset);
    }
    return 0xF9000000 | ((byte_offset / 8) << 10) | (rn << 5) | rt;
}

inline std::uint32_t imm7(std::int32_t byte_offset) {
    if (byte_offset % 8 != 0 || byte_offset < -512 || byte_offset > 504) {
        fail("pair offset {} is not encodable", byte_offset);
    }
    return static_cast<std::uint32_t>(byte_offset / 8) & 0x7F;
}
inline std::uint32_t stp_x(std::uint32_t rt1, std::uint32_t rt2, std::uint32_t rn, std::int32_t off) {
    return 0xA9000000 | (imm7(off) << 15) | (rt2 << 10) | (rn << 5) | rt1;
}
inline std::uint32_t ldp_x(std::uint32_t rt1, std::uint32_t rt2, std::uint32_t rn, std::int32_t off) {
    return 0xA9400000 | (imm7(off) << 15) | (rt2 << 10) | (rn << 5) | rt1;
}
inline std::uint32_t stp_x_pre(std::uint32_t rt1, std::uint32_t rt2, std::uint32_t rn, std::int32_t off) {
    return 0xA9800000 | (imm7(off) << 15) | (rt2 << 10) | (rn << 5) | rt1;
}
inline std::uint32_t ldp_x_post(std::uint32_t rt1, std::uint32_t rt2, std::uint32_t rn, std::int32_t off) {
    return 0xA8C00000 | (imm7(off) << 15) | (rt2 << 10) | (rn << 5) | rt1;
}
inline std::uint32_t imm9(std::int32_t off) {
    if (off < -256 || off > 255) {
        fail("offset {} is not encodable in imm9", off);
    }
    return static_cast<std::uint32_t>(off) & 0x1FF;
}
inline std::uint32_t str_x_pre(std::uint32_t rt, std::uint32_t rn, std::int32_t off) {
    return 0xF8000C00 | (imm9(off) << 12) | (rn << 5) | rt;
}
inline std::uint32_t ldr_x_post(std::uint32_t rt, std::uint32_t rn, std::int32_t off) {
    return 0xF8400400 | (imm9(off) << 12) | (rn << 5) | rt;
}

constexpr std::uint32_t mrs(std::uint32_t rt, std::uint32_t reg) { return 0xD5200000 | reg | rt; }
constexpr std::uint32_t msr(std::uint32_t reg, std::uint32_t rt) { return 0xD5000000 | reg | rt; }

// ---- decoders --------------------------------------------------------------

constexpr std::optional<std::uint32_t> decode_svc(std::uint32_t insn) {
    if ((insn & 0xFFE0001F) == 0xD4000001) {
        return (insn >> 5) & 0xFFFF;
    }
    return std::nullopt;
}

struct SysRegMove {
    bool is_read; // MRS (true) or MSR register form (false)
    std::uint32_t reg;
    std::uint32_t rt;
};

constexpr std::optional<SysRegMove> decode_sysreg_move(std::uint32_t insn) {
    if ((insn & 0xFFF00000) == 0xD5300000) {
        return SysRegMove{true, insn & kSysregMask, insn & 31};
    }
    if ((insn & 0xFFF00000) == 0xD5100000) {
        return SysRegMove{false, insn & kSysregMask, insn & 31};
    }
    return std::nullopt;
}

struct SysOp {
    std::uint32_t op1, crn, crm, op2, rt;
};

// SYS alias space (DC/IC/AT/TLBI): op0 == 1.
constexpr std::optional<SysOp> decode_sys(std::uint32_t insn) {
    if ((insn & 0xFFF80000) == 0xD5080000) {
        return SysOp{(insn >> 16) & 7, (insn >> 12) & 15, (insn >> 8) & 15, (insn >> 5) & 7, insn & 31};
    }
    return std::nullopt;
}

enum class ExceptionKind { None, Svc, Hvc, Smc, Brk, Hlt };

constexpr ExceptionKind decode_exception(std::uint32_t insn) {
    switch (insn & 0xFFE0001F) {
    case 0xD4000001: return ExceptionKind::Svc;
    case 0xD4000002: return ExceptionKind::Hvc;
    case 0xD4000003: return ExceptionKind::Smc;
    case 0xD4200000: return ExceptionKind::Brk;
    case 0xD4400000: return ExceptionKind::Hlt;
    default: return ExceptionKind::None;
    }
}

// Load/store exclusive register class with o2 == 0: LDXR/STXR/LDAXR/STLXR and
// the 64/32-bit pair forms. CASP (size bit 31 clear with o1 set) is excluded.
constexpr bool is_exclusive(std::uint32_t insn) {
    if ((insn & 0x3F800000) != 0x08000000) {
        return false;
    }
    bool pair = (insn >> 21) & 1;
    return !pair || ((insn >> 31) & 1);
}
constexpr bool is_ordered_exclusive(std::uint32_t insn) { return (insn >> 15) & 1; }
constexpr std::uint32_t make_ordered_exclusive(std::uint32_t insn) { return insn | (1u << 15); }

} // namespace nx::a64
