// SPDX-License-Identifier: MIT
// How the runtime exports functions and data to relinked guest modules.
//
// Guest modules import "nx$" + <original symbol name>. Define an export with
//
//   NX_GUEST_FN(int, libc_puts, "puts", (const char* s)) { ... }
//
// The second argument is the C++ name used inside the runtime; the third is the
// exact (mangled) symbol name the game imports. Mangled names are verified by
// the integration corpus, which calls every export through real C++ prototypes
// (tests/corpus/include) and fails if a name does not resolve.
#pragma once

#include "nxabi/nxabi.h"

#include <cstdint>

#define NX_GUEST_FN(ret, cname, symbol, params)                                                     \
    extern "C" __attribute__((visibility("default"))) ret cname params __asm__(NXABI_SYMBOL_PREFIX symbol); \
    extern "C" __attribute__((used)) ret cname params

#define NX_GUEST_VAR(type, cname, symbol)                                                            \
    extern "C" __attribute__((visibility("default"))) type cname __asm__(NXABI_SYMBOL_PREFIX symbol); \
    __attribute__((used)) type cname

namespace nxrt::guest {

// Horizon result code: module in bits 0-8, description in bits 9-21.
using Result = std::uint32_t;
constexpr Result make_result(std::uint32_t module, std::uint32_t description) {
    return (module & 0x1FF) | ((description & 0x1FFF) << 9);
}
constexpr Result kSuccess = 0;
constexpr Result kInvalidEnumValue = make_result(1, 120);
constexpr Result kInvalidSize = make_result(1, 101);

// nn::TimeSpan and nn::os::Tick are trivially copyable 8-byte classes and are
// passed and returned in x0 under AAPCS64.
struct TimeSpan {
    std::int64_t ns;
};
struct Tick {
    std::int64_t value;
};

} // namespace nxrt::guest
