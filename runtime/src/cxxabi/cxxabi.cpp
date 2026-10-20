// SPDX-License-Identifier: MIT
// Itanium C++ ABI support routines that games import from the SDK's libc++abi.
// The ABI is the same on Horizon and Linux AArch64, so the host libc++abi
// implementations are used where they exist.
#include "nxrt/guest.hpp"
#include "nxrt/runtime.hpp"

#include <cstdint>
#include <cstdlib>
#include <malloc.h>
#include <sys/random.h>

extern "C" int __cxa_atexit(void (*fn)(void*), void* arg, void* dso);
extern "C" int __cxa_guard_acquire(std::uint64_t* guard);
extern "C" void __cxa_guard_release(std::uint64_t* guard);
extern "C" void __cxa_guard_abort(std::uint64_t* guard);

NX_GUEST_FN(int, cxa_atexit, "__cxa_atexit", (void (*fn)(void*), void* arg, void* dso)) {
    return __cxa_atexit(fn, arg, dso);
}
NX_GUEST_FN(int, cxa_guard_acquire, "__cxa_guard_acquire", (std::uint64_t* guard)) {
    return __cxa_guard_acquire(guard);
}
NX_GUEST_FN(void, cxa_guard_release, "__cxa_guard_release", (std::uint64_t* guard)) { __cxa_guard_release(guard); }
NX_GUEST_FN(void, cxa_guard_abort, "__cxa_guard_abort", (std::uint64_t* guard)) { __cxa_guard_abort(guard); }
NX_GUEST_FN(void, cxa_pure_virtual, "__cxa_pure_virtual", ()) {
    nxrt::fatal(nxrt::kExitGuestCrash, "guest called a pure virtual function");
}
NX_GUEST_FN(void, cxa_deleted_virtual, "__cxa_deleted_virtual", ()) {
    nxrt::fatal(nxrt::kExitGuestCrash, "guest called a deleted virtual function");
}

namespace {
void* checked(void* p, std::size_t n) {
    if (!p && n) {
        nxrt::fatal(nxrt::kExitGuestCrash, "guest operator new(%zu) failed: out of memory", n);
    }
    return p;
}
} // namespace

NX_GUEST_FN(void*, cxx_new, "_Znwm", (std::size_t n)) { return checked(std::malloc(n ? n : 1), n); }
NX_GUEST_FN(void*, cxx_new_array, "_Znam", (std::size_t n)) { return checked(std::malloc(n ? n : 1), n); }
NX_GUEST_FN(void*, cxx_new_nothrow, "_ZnwmRKSt9nothrow_t", (std::size_t n, const void*)) { return std::malloc(n ? n : 1); }
NX_GUEST_FN(void*, cxx_new_array_nothrow, "_ZnamRKSt9nothrow_t", (std::size_t n, const void*)) {
    return std::malloc(n ? n : 1);
}
NX_GUEST_FN(void*, cxx_new_aligned, "_ZnwmSt11align_val_t", (std::size_t n, std::size_t align)) {
    return checked(memalign(align, n ? n : 1), n);
}
NX_GUEST_FN(void, cxx_delete, "_ZdlPv", (void* p)) { std::free(p); }
NX_GUEST_FN(void, cxx_delete_array, "_ZdaPv", (void* p)) { std::free(p); }
NX_GUEST_FN(void, cxx_delete_sized, "_ZdlPvm", (void* p, std::size_t)) { std::free(p); }
NX_GUEST_FN(void, cxx_delete_array_sized, "_ZdaPvm", (void* p, std::size_t)) { std::free(p); }
NX_GUEST_FN(void, cxx_delete_aligned, "_ZdlPvSt11align_val_t", (void* p, std::size_t)) { std::free(p); }

NX_GUEST_VAR(std::uintptr_t, libc_stack_chk_guard, "__stack_chk_guard") = 0x5A17C4A9E0D1F00Dull;

namespace {
__attribute__((constructor)) void init_stack_guard() {
    std::uintptr_t v = 0;
    if (getrandom(&v, sizeof v, 0) == sizeof v && v != 0) {
        libc_stack_chk_guard = v;
    }
}
} // namespace

NX_GUEST_FN(void, libc_stack_chk_fail, "__stack_chk_fail", ()) {
    nxrt::fatal(nxrt::kExitGuestCrash, "guest stack smashing detected");
}
