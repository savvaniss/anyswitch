// SPDX-License-Identifier: MIT
// Raw supervisor calls, as homebrew and inlined helpers issue them. Register
// conventions: https://switchbrew.org/wiki/SVC
#include "guest/libc.h"

namespace {

unsigned svc_output_debug_string(const char* s, unsigned long n) {
    register unsigned long x0 asm("x0") = reinterpret_cast<unsigned long>(s);
    register unsigned long x1 asm("x1") = n;
    asm volatile("svc #0x27" : "+r"(x0), "+r"(x1)::"x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11",
                 "x12", "x13", "x14", "x15", "x16", "x17", "memory");
    return static_cast<unsigned>(x0);
}

unsigned long svc_get_system_tick() {
    register unsigned long x0 asm("x0");
    asm volatile("svc #0x1E" : "=r"(x0)::"x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12",
                 "x13", "x14", "x15", "x16", "x17", "memory");
    return x0;
}

void svc_sleep_thread(long long ns) {
    register long long x0 asm("x0") = ns;
    asm volatile("svc #0x0B" : "+r"(x0)::"x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12",
                 "x13", "x14", "x15", "x16", "x17", "memory");
}

unsigned svc_get_info(unsigned type, unsigned long subtype, unsigned long* out) {
    register unsigned long x0 asm("x0") = 0;
    register unsigned long x1 asm("x1") = type;
    register unsigned long x2 asm("x2") = 0;
    register unsigned long x3 asm("x3") = subtype;
    asm volatile("svc #0x29" : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3)::"x4", "x5", "x6", "x7", "x8", "x9", "x10",
                 "x11", "x12", "x13", "x14", "x15", "x16", "x17", "memory");
    *out = x1;
    return static_cast<unsigned>(x0);
}

bool flags_survive_svc() {
    unsigned long eq;
    asm volatile("cmp xzr, xzr\n"  // Z = 1
                 "svc #0x1E\n"
                 "cset %0, eq\n"
                 : "=r"(eq)::"x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13",
                   "x14", "x15", "x16", "x17", "cc", "memory");
    return eq == 1;
}

[[noreturn]] void svc_exit_process() {
    asm volatile("svc #0x07" ::: "memory");
    __builtin_unreachable();
}

} // namespace

extern "C" void nnMain() {
    const char text[] = "hello via svc";
    unsigned r = svc_output_debug_string(text, sizeof text - 1);
    printf("svc output result %u\n", r);

    unsigned long a = svc_get_system_tick();
    svc_sleep_thread(20 * 1000 * 1000);
    unsigned long b = svc_get_system_tick();
    printf("svc tick %s\n", b > a && b - a >= 300000 ? "ok" : "BAD");

    unsigned long program_id = 0;
    unsigned rc = svc_get_info(18, 0, &program_id);
    printf("program id %016lx rc %u\n", program_id, rc);
    unsigned long bogus = 0;
    printf("bad info rc %#x\n", svc_get_info(0x7777, 0, &bogus));

    printf("flags %s\n", flags_survive_svc() ? "preserved" : "BAD");
    puts("about to exit");
    fflush(nullptr);
    svc_exit_process();
    puts("should not print");
}
