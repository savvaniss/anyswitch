// SPDX-License-Identifier: MIT
// Instructions the relinker patches: tpidrro_el0 / tpidr_el0 / counters /
// exclusives. Each check prints one line the runner matches.
#include "guest/libc.h"
#include "nn/os.hpp"

namespace {

unsigned long read_tlr() {
    unsigned long v;
    asm volatile("mrs %0, tpidrro_el0" : "=r"(v));
    return v;
}

unsigned long read_tp() {
    unsigned long v;
    asm volatile("mrs %0, tpidr_el0" : "=r"(v));
    return v;
}

void write_tp(unsigned long v) { asm volatile("msr tpidr_el0, %0" ::"r"(v)); }

unsigned long read_counter() {
    unsigned long v;
    asm volatile("isb; mrs %0, cntpct_el0" : "=r"(v));
    return v;
}

unsigned long read_counter_into_x0() {
    register unsigned long x0 asm("x0");
    asm volatile("mrs x0, cntvct_el0" : "=r"(x0));
    return x0;
}

unsigned long read_frequency() {
    unsigned long v;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(v));
    return v;
}

int exclusive_add(int* p, int v) {
    int old, sum, status;
    asm volatile("1: ldxr %w0, [%3]\n"
                 "   add %w1, %w0, %w4\n"
                 "   stxr %w2, %w1, [%3]\n"
                 "   cbnz %w2, 1b\n"
                 : "=&r"(old), "=&r"(sum), "=&r"(status)
                 : "r"(p), "r"(v)
                 : "memory");
    return old;
}

} // namespace

extern "C" void nnMain() {
    unsigned long tlr = read_tlr();
    auto* region = reinterpret_cast<unsigned char*>(tlr);
    void* thread_type = nullptr;
    if (tlr) memcpy(&thread_type, region + 0x1F8, sizeof thread_type);
    bool tlr_ok = tlr != 0 && (tlr & 0x1FF) == 0 && thread_type == nn::os::GetCurrentThread();
    if (tlr_ok) {
        region[0] = 0x5A;
        tlr_ok = reinterpret_cast<volatile unsigned char*>(read_tlr())[0] == 0x5A;
    }
    printf("tlr %s\n", tlr_ok ? "ok" : "BAD");

    write_tp(0x1234567890ull);
    printf("tpidr %s\n", read_tp() == 0x1234567890ull ? "ok" : "BAD");

    printf("cntfrq %lu\n", read_frequency());

    unsigned long t0 = read_counter();
    nn::os::SleepThread(nn::TimeSpan::FromMilliSeconds(100));
    unsigned long t1 = read_counter_into_x0();
    unsigned long delta = t1 - t0;
    bool tick_ok = delta >= 1800000 && delta < 19200000; // 100 ms at 19.2 MHz, generous upper bound
    long long ns = nn::os::ConvertToTimeSpan(nn::os::Tick(static_cast<long long>(delta))).GetNanoSeconds();
    printf("tick %s\n", tick_ok && ns >= 95000000 ? "ok" : "BAD");
    if (!tick_ok) printf("  delta=%lu ns=%lld\n", delta, ns);
    printf("tick frequency %lld\n", nn::os::GetSystemTickFrequency());

    int counter = 0;
    for (int i = 0; i < 1000; ++i) exclusive_add(&counter, 3);
    printf("exclusive %s\n", counter == 3000 ? "ok" : "BAD");
}
