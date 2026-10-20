// SPDX-License-Identifier: MIT
// Guest heap entry points. Forwarded to the host allocator for now; a
// dedicated guest arena is tracked in docs/dev/debt/libc.md.
#include "nxrt/guest.hpp"

#include <cstdlib>
#include <malloc.h>

NX_GUEST_FN(void*, libc_malloc, "malloc", (std::size_t n)) { return std::malloc(n); }
NX_GUEST_FN(void, libc_free, "free", (void* p)) { std::free(p); }
NX_GUEST_FN(void*, libc_calloc, "calloc", (std::size_t n, std::size_t size)) { return std::calloc(n, size); }
NX_GUEST_FN(void*, libc_realloc, "realloc", (void* p, std::size_t n)) { return std::realloc(p, n); }
NX_GUEST_FN(void*, libc_aligned_alloc, "aligned_alloc", (std::size_t align, std::size_t n)) {
    return memalign(align, n);
}
NX_GUEST_FN(void*, libc_memalign, "memalign", (std::size_t align, std::size_t n)) { return memalign(align, n); }
NX_GUEST_FN(int, libc_posix_memalign, "posix_memalign", (void** out, std::size_t align, std::size_t n)) {
    return posix_memalign(out, align, n);
}
NX_GUEST_FN(std::size_t, libc_malloc_usable_size, "malloc_usable_size", (void* p)) { return malloc_usable_size(p); }
