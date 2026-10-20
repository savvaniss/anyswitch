// SPDX-License-Identifier: MIT
// Expected words are what GNU as / LLVM MC emit for the instruction in the comment.
#include "test.hpp"

#include "nx/relinker/a64.hpp"

using namespace nx::a64;

NX_TEST(a64_system_registers) {
    NX_CHECK_EQ(mrs(0, kTpidrroEl0), 0xD53BD060); // mrs x0, tpidrro_el0
    NX_CHECK_EQ(mrs(1, kTpidrEl0), 0xD53BD041);   // mrs x1, tpidr_el0
    NX_CHECK_EQ(msr(kTpidrEl0, 2), 0xD51BD042);   // msr tpidr_el0, x2
    NX_CHECK_EQ(mrs(0, kCntpctEl0), 0xD53BE020);  // mrs x0, cntpct_el0
    NX_CHECK_EQ(mrs(0, kCntvctEl0), 0xD53BE040);  // mrs x0, cntvct_el0
    NX_CHECK_EQ(mrs(0, kCntfrqEl0), 0xD53BE000);  // mrs x0, cntfrq_el0
    NX_CHECK_EQ(mrs(9, kNzcv), 0xD53B4209);       // mrs x9, nzcv
    NX_CHECK_EQ(msr(kNzcv, 9), 0xD51B4209);       // msr nzcv, x9
}

NX_TEST(a64_loads_stores) {
    NX_CHECK_EQ(stp_x_pre(16, 17, kSp, -16), 0xA9BF47F0); // stp x16, x17, [sp, #-16]!
    NX_CHECK_EQ(ldp_x_post(16, 17, kSp, 16), 0xA8C147F0); // ldp x16, x17, [sp], #16
    NX_CHECK_EQ(str_x_pre(30, kSp, -16), 0xF81F0FFE);     // str x30, [sp, #-16]!
    NX_CHECK_EQ(ldr_x_post(30, kSp, 16), 0xF84107FE);     // ldr x30, [sp], #16
    NX_CHECK_EQ(stp_x(0, 1, kSp, 0), 0xA90007E0);         // stp x0, x1, [sp]
    NX_CHECK_EQ(ldp_x(0, 1, kSp, 0), 0xA94007E0);         // ldp x0, x1, [sp]
    NX_CHECK_EQ(stp_x(18, 30, kSp, 64), 0xA9047BF2);      // stp x18, x30, [sp, #64]
    NX_CHECK_EQ(ldr_x(16, 16, 8), 0xF9400610);            // ldr x16, [x16, #8]
    NX_CHECK_EQ(ldr_x(5, 5, 64), 0xF94020A5);             // ldr x5, [x5, #64]
    NX_CHECK_EQ(str_x(9, kSp, 80), 0xF9002BE9);           // str x9, [sp, #80]
    NX_CHECK_THROWS(ldr_x(0, 1, 4));
    NX_CHECK_THROWS(stp_x(0, 1, kSp, 1024));
}

NX_TEST(a64_arithmetic_and_moves) {
    NX_CHECK_EQ(sub_imm(kSp, kSp, 96), 0xD10183FF); // sub sp, sp, #96
    NX_CHECK_EQ(add_imm(kSp, kSp, 96), 0x910183FF); // add sp, sp, #96
    NX_CHECK_EQ(mov_from_sp(0), 0x910003E0);        // mov x0, sp
    NX_CHECK_EQ(mov_reg(2, 0), 0xAA0003E2);         // mov x2, x0
    NX_CHECK_EQ(mov_reg_w(1, 16), 0x2A1003E1);      // mov w1, w16
    NX_CHECK_EQ(movz_w(16, 0x27, 0), 0x528004F0);   // mov w16, #0x27
    NX_CHECK_EQ(movz_x(5, 0xF800, 0), 0xD29F0005);  // mov x5, #0xf800
    NX_CHECK_EQ(movk_x(5, 0x124, 16), 0xF2A02485);  // movk x5, #0x124, lsl #16
    NX_CHECK_EQ(umulh(2, 0, 2), 0x9BC27C02);        // umulh x2, x0, x2
    NX_CHECK_EQ(mul(1, 0, 1), 0x9B017C01);          // mul x1, x0, x1
    NX_CHECK_EQ(add_reg(0, 1, 2), 0x8B020020);      // add x0, x1, x2
}

NX_TEST(a64_branches) {
    NX_CHECK_EQ(b(0x1000, 0x1008), 0x14000002);
    NX_CHECK_EQ(b(0x1000, 0x0FFC), 0x17FFFFFF);
    NX_CHECK_EQ(bl(0x1000, 0x1100), 0x94000040);
    NX_CHECK_EQ(br(16), 0xD61F0200);  // br x16
    NX_CHECK_EQ(blr(9), 0xD63F0120);  // blr x9
    NX_CHECK_EQ(adrp(1, 0x10000, 0x23456), 0xF0000081);
    NX_CHECK_THROWS(b(0, 0x8000000));
    NX_CHECK_THROWS(b(0, 2));
}

NX_TEST(a64_decoders) {
    NX_CHECK_EQ(decode_svc(0xD40004E1).value(), 0x27); // svc #0x27
    NX_CHECK(!decode_svc(0xD4000002));                  // hvc #0
    auto mv = decode_sysreg_move(0xD53BD060);
    NX_CHECK(mv && mv->is_read && mv->reg == kTpidrroEl0 && mv->rt == 0);
    auto wr = decode_sysreg_move(0xD51BD042);
    NX_CHECK(wr && !wr->is_read && wr->reg == kTpidrEl0 && wr->rt == 2);
    NX_CHECK(is_exclusive(0xC85F7C20));                  // ldxr x0, [x1]
    NX_CHECK(!is_ordered_exclusive(0xC85F7C20));
    NX_CHECK_EQ(make_ordered_exclusive(0xC85F7C20), 0xC85FFC20); // ldaxr x0, [x1]
    NX_CHECK_EQ(make_ordered_exclusive(0xC8027C20), 0xC802FC20); // stxr w2 -> stlxr w2
    NX_CHECK(is_exclusive(0xC87F0440));                  // ldxp x0, x1, [x2]
    NX_CHECK(!is_exclusive(0x48207C82));                 // casp x0, x1, x2, x3, [x4]
    NX_CHECK(!is_exclusive(0xC8DFFC20));                 // ldar x0, [x1]
    NX_CHECK(decode_exception(0xD4200000) == ExceptionKind::Brk);
}
