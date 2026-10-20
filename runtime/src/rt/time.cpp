// SPDX-License-Identifier: MIT
// Guest time runs at 19.2 MHz (https://switchbrew.org/wiki/SVC, GetSystemTick).
// Host counters run at other rates (24 MHz on Apple silicon, 54 MHz on a
// Raspberry Pi 5, 1 GHz on Armv8.6+ cores), so patched counter reads rescale.
#include "nxrt/runtime.hpp"

namespace nxrt {
namespace {

std::uint64_t g_mul_int = 0;
std::uint64_t g_mul_frac = 0;

std::uint64_t host_counter() {
    std::uint64_t v;
    asm volatile("isb; mrs %0, cntvct_el0" : "=r"(v));
    return v;
}

std::uint64_t host_frequency() {
    std::uint64_t v;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(v));
    return v;
}

} // namespace

void init_tick_scale(std::uint64_t& mul_int, std::uint64_t& mul_frac) {
    if (g_mul_int == 0 && g_mul_frac == 0) {
        std::uint64_t freq = host_frequency();
        if (freq == 0) {
            fatal(1, "host reports a zero counter frequency");
        }
        g_mul_int = NXABI_GUEST_TICK_HZ / freq;
        unsigned __int128 rem = NXABI_GUEST_TICK_HZ % freq;
        g_mul_frac = static_cast<std::uint64_t>((rem << 64) / freq);
        log("host counter %llu Hz, guest tick scale %llu + %#llx/2^64", static_cast<unsigned long long>(freq),
            static_cast<unsigned long long>(g_mul_int), static_cast<unsigned long long>(g_mul_frac));
    }
    mul_int = g_mul_int;
    mul_frac = g_mul_frac;
}

std::uint64_t guest_ticks_now() {
    std::uint64_t mi, mf;
    init_tick_scale(mi, mf);
    std::uint64_t c = host_counter();
    return c * mi + static_cast<std::uint64_t>((static_cast<unsigned __int128>(c) * mf) >> 64);
}

std::int64_t ticks_to_ns(std::int64_t ticks) {
    // 1e9 / 19.2e6 == 625 / 12
    return static_cast<std::int64_t>((static_cast<__int128>(ticks) * 625) / 12);
}

} // namespace nxrt
