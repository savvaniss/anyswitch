// SPDX-License-Identifier: MIT
// References every guest export the runtime provides through a compiler-
// generated prototype, so a wrong mangled name in runtime/src shows up as a
// trapped or unresolved import (AGENTS.md rule E4). The runner's coverage
// check fails if an export is referenced by no corpus case at all.
// A few calls check that forwarding preserves arguments and results.
#include "guest/libc.h"
#include "nn/os.hpp"

namespace {

using AnyFn = void (*)();

template <class F>
AnyFn erase(F f) {
    return reinterpret_cast<AnyFn>(f);
}

int compare_ints(const void* a, const void* b) {
    return *static_cast<const int*>(a) - *static_cast<const int*>(b);
}

int call_vsnprintf(char* out, size_t n, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(out, n, fmt, ap);
    va_end(ap);
    return r;
}

} // namespace

extern "C" void nnMain() {
    using nn::os::ThreadType;
    using nn::os::ThreadFunction;
    volatile AnyFn functions[] = {
        // stdio
        erase(&printf), erase(&fprintf), erase(&sprintf), erase(&snprintf), erase(&vprintf), erase(&vfprintf),
        erase(&vsprintf), erase(&vsnprintf), erase(&puts), erase(&putchar), erase(&fputs), erase(&fputc),
        erase(&fwrite), erase(&fflush),
        // memory
        erase(&malloc), erase(&free), erase(&calloc), erase(&realloc), erase(&aligned_alloc), erase(&memalign),
        erase(&posix_memalign), erase(&malloc_usable_size),
        // strings
        erase(&memcpy), erase(&memmove), erase(&memset), erase(&memcmp), erase(&memchr), erase(&strlen),
        erase(&strnlen), erase(&strcmp), erase(&strncmp), erase(&strcpy), erase(&strncpy), erase(&strcat),
        erase(&strncat), erase(&strchr), erase(&strrchr), erase(&strstr), erase(&strdup), erase(&strtol),
        erase(&strtoul), erase(&strtoll), erase(&strtoull), erase(&strtod), erase(&strtof), erase(&atoi),
        erase(&atol),
        // process
        erase(&abort), erase(&exit), erase(&_Exit), erase(&atexit), erase(&__errno_location), erase(&rand),
        erase(&srand), erase(&qsort),
        // C++ ABI
        erase(&__cxa_atexit), erase(&__cxa_guard_acquire), erase(&__cxa_guard_release), erase(&__cxa_guard_abort),
        erase(&__cxa_pure_virtual), erase(&__cxa_deleted_virtual), erase(&__stack_chk_fail),
        erase(static_cast<void* (*)(size_t)>(&::operator new)),
        erase(static_cast<void* (*)(size_t)>(&::operator new[])),
        erase(static_cast<void* (*)(size_t, const std::nothrow_t&) noexcept>(&::operator new)),
        erase(static_cast<void* (*)(size_t, const std::nothrow_t&) noexcept>(&::operator new[])),
        erase(static_cast<void* (*)(size_t, std::align_val_t)>(&::operator new)),
        erase(static_cast<void (*)(void*) noexcept>(&::operator delete)),
        erase(static_cast<void (*)(void*) noexcept>(&::operator delete[])),
        erase(static_cast<void (*)(void*, size_t) noexcept>(&::operator delete)),
        erase(static_cast<void (*)(void*, size_t) noexcept>(&::operator delete[])),
        erase(static_cast<void (*)(void*, std::align_val_t) noexcept>(&::operator delete)),
        // nn::os
        erase(&nn::os::GetSystemTick), erase(&nn::os::GetSystemTickFrequency), erase(&nn::os::ConvertToTimeSpan),
        erase(&nn::os::SleepThread), erase(&nn::os::YieldThread), erase(&nn::os::GetHostArgc),
        erase(&nn::os::GetHostArgv),
        erase(static_cast<nn::Result (*)(ThreadType*, ThreadFunction, void*, void*, unsigned long, int)>(
            &nn::os::CreateThread)),
        erase(static_cast<nn::Result (*)(ThreadType*, ThreadFunction, void*, void*, unsigned long, int, int)>(
            &nn::os::CreateThread)),
        erase(&nn::os::StartThread), erase(&nn::os::WaitThread), erase(&nn::os::DestroyThread),
        erase(&nn::os::GetCurrentThread), erase(&nn::os::InitializeMutex), erase(&nn::os::FinalizeMutex),
        erase(&nn::os::LockMutex), erase(&nn::os::TryLockMutex), erase(&nn::os::UnlockMutex),
    };
    const void* volatile data[] = {&stdin, &stdout, &stderr, &__stack_chk_guard};

    int null_entries = 0;
    for (auto f : functions) null_entries += f == nullptr;
    for (auto d : data) null_entries += d == nullptr;
    printf("surface %d functions %d data, %d null\n", static_cast<int>(sizeof functions / sizeof functions[0]),
           static_cast<int>(sizeof data / sizeof data[0]), null_entries);

    char* end = nullptr;
    char buffer[64];
    call_vsnprintf(buffer, sizeof buffer, "%ld/%llu/%d", strtol("-42", &end, 10), strtoull("ff", nullptr, 16),
                   atoi("7"));
    printf("numbers %s\n", buffer);
    int values[] = {5, 3, 9, 1};
    qsort(values, 4, sizeof(int), compare_ints);
    printf("sorted %d %d %d %d\n", values[0], values[1], values[2], values[3]);
    char* copy = strdup("relinked");
    printf("strings %s %d %s\n", strstr(copy, "link"), static_cast<int>(strnlen(copy, 4)), strrchr(copy, 'e'));
    free(copy);
    void* aligned = aligned_alloc(256, 512);
    printf("aligned %s\n", (reinterpret_cast<uintptr_t>(aligned) & 255) == 0 ? "ok" : "BAD");
    free(aligned);
    fprintf(stderr, "stderr ok\n");
    printf("stack guard %s\n", __stack_chk_guard != 0 ? "set" : "BAD");
}
