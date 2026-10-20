// SPDX-License-Identifier: MIT
#include "nxrt/guest.hpp"
#include "nxrt/runtime.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>

NX_GUEST_FN(void, libc_abort, "abort", ()) {
    nxrt::fatal(nxrt::kExitGuestCrash, "guest called abort()");
}
NX_GUEST_FN(void, libc_exit, "exit", (int code)) { std::exit(code); }
NX_GUEST_FN(void, libc__Exit, "_Exit", (int code)) {
    std::fflush(nullptr);
    std::_Exit(code);
}
NX_GUEST_FN(int, libc_atexit, "atexit", (void (*fn)())) { return std::atexit(fn); }
NX_GUEST_FN(int*, libc___errno_location, "__errno_location", ()) { return &errno; }
NX_GUEST_FN(int, libc_rand, "rand", ()) { return std::rand(); }
NX_GUEST_FN(void, libc_srand, "srand", (unsigned seed)) { std::srand(seed); }
NX_GUEST_FN(void, libc_qsort, "qsort",
            (void* base, std::size_t n, std::size_t size, int (*cmp)(const void*, const void*))) {
    std::qsort(base, n, size, cmp);
}
