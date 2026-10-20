// SPDX-License-Identifier: MIT
// stdio. Guest FILE* values are host FILE* values: the guest only ever passes
// them back to these functions. va_list has the same AAPCS64 layout on
// Horizon and Linux, so the v* variants forward unchanged.
#include "nxrt/guest.hpp"

#include <cstdarg>
#include <cstdio>

NX_GUEST_VAR(FILE*, libc_stdin, "stdin") = nullptr;
NX_GUEST_VAR(FILE*, libc_stdout, "stdout") = nullptr;
NX_GUEST_VAR(FILE*, libc_stderr, "stderr") = nullptr;

namespace {
__attribute__((constructor)) void init_guest_stdio() {
    libc_stdin = stdin;
    libc_stdout = stdout;
    libc_stderr = stderr;
}
} // namespace

NX_GUEST_FN(int, libc_vprintf, "vprintf", (const char* fmt, va_list ap)) { return std::vprintf(fmt, ap); }
NX_GUEST_FN(int, libc_vfprintf, "vfprintf", (FILE* f, const char* fmt, va_list ap)) {
    return std::vfprintf(f, fmt, ap);
}
NX_GUEST_FN(int, libc_vsprintf, "vsprintf", (char* out, const char* fmt, va_list ap)) {
    return std::vsprintf(out, fmt, ap);
}
NX_GUEST_FN(int, libc_vsnprintf, "vsnprintf", (char* out, std::size_t n, const char* fmt, va_list ap)) {
    return std::vsnprintf(out, n, fmt, ap);
}

NX_GUEST_FN(int, libc_printf, "printf", (const char* fmt, ...)) {
    va_list ap;
    va_start(ap, fmt);
    int r = std::vprintf(fmt, ap);
    va_end(ap);
    return r;
}
NX_GUEST_FN(int, libc_fprintf, "fprintf", (FILE* f, const char* fmt, ...)) {
    va_list ap;
    va_start(ap, fmt);
    int r = std::vfprintf(f, fmt, ap);
    va_end(ap);
    return r;
}
NX_GUEST_FN(int, libc_sprintf, "sprintf", (char* out, const char* fmt, ...)) {
    va_list ap;
    va_start(ap, fmt);
    int r = std::vsprintf(out, fmt, ap);
    va_end(ap);
    return r;
}
NX_GUEST_FN(int, libc_snprintf, "snprintf", (char* out, std::size_t n, const char* fmt, ...)) {
    va_list ap;
    va_start(ap, fmt);
    int r = std::vsnprintf(out, n, fmt, ap);
    va_end(ap);
    return r;
}

NX_GUEST_FN(int, libc_puts, "puts", (const char* s)) { return std::puts(s); }
NX_GUEST_FN(int, libc_putchar, "putchar", (int c)) { return std::putchar(c); }
NX_GUEST_FN(int, libc_fputs, "fputs", (const char* s, FILE* f)) { return std::fputs(s, f); }
NX_GUEST_FN(int, libc_fputc, "fputc", (int c, FILE* f)) { return std::fputc(c, f); }
NX_GUEST_FN(std::size_t, libc_fwrite, "fwrite", (const void* p, std::size_t size, std::size_t n, FILE* f)) {
    return std::fwrite(p, size, n, f);
}
NX_GUEST_FN(int, libc_fflush, "fflush", (FILE* f)) { return std::fflush(f); }
