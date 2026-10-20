// SPDX-License-Identifier: MIT
// String and memory functions. Horizon's libc is musl; these have identical
// contracts in glibc, so they forward directly.
#include "nxrt/guest.hpp"

#include <cstdlib>
#include <cstring>

NX_GUEST_FN(void*, libc_memcpy, "memcpy", (void* d, const void* s, std::size_t n)) { return std::memcpy(d, s, n); }
NX_GUEST_FN(void*, libc_memmove, "memmove", (void* d, const void* s, std::size_t n)) { return std::memmove(d, s, n); }
NX_GUEST_FN(void*, libc_memset, "memset", (void* d, int c, std::size_t n)) { return std::memset(d, c, n); }
NX_GUEST_FN(int, libc_memcmp, "memcmp", (const void* a, const void* b, std::size_t n)) { return std::memcmp(a, b, n); }
NX_GUEST_FN(const void*, libc_memchr, "memchr", (const void* s, int c, std::size_t n)) { return std::memchr(s, c, n); }
NX_GUEST_FN(std::size_t, libc_strlen, "strlen", (const char* s)) { return std::strlen(s); }
NX_GUEST_FN(std::size_t, libc_strnlen, "strnlen", (const char* s, std::size_t n)) { return strnlen(s, n); }
NX_GUEST_FN(int, libc_strcmp, "strcmp", (const char* a, const char* b)) { return std::strcmp(a, b); }
NX_GUEST_FN(int, libc_strncmp, "strncmp", (const char* a, const char* b, std::size_t n)) { return std::strncmp(a, b, n); }
NX_GUEST_FN(char*, libc_strcpy, "strcpy", (char* d, const char* s)) { return std::strcpy(d, s); }
NX_GUEST_FN(char*, libc_strncpy, "strncpy", (char* d, const char* s, std::size_t n)) { return std::strncpy(d, s, n); }
NX_GUEST_FN(char*, libc_strcat, "strcat", (char* d, const char* s)) { return std::strcat(d, s); }
NX_GUEST_FN(char*, libc_strncat, "strncat", (char* d, const char* s, std::size_t n)) { return std::strncat(d, s, n); }
NX_GUEST_FN(const char*, libc_strchr, "strchr", (const char* s, int c)) { return std::strchr(s, c); }
NX_GUEST_FN(const char*, libc_strrchr, "strrchr", (const char* s, int c)) { return std::strrchr(s, c); }
NX_GUEST_FN(const char*, libc_strstr, "strstr", (const char* h, const char* n)) { return std::strstr(h, n); }
NX_GUEST_FN(char*, libc_strdup, "strdup", (const char* s)) { return strdup(s); }
NX_GUEST_FN(long, libc_strtol, "strtol", (const char* s, char** end, int base)) { return std::strtol(s, end, base); }
NX_GUEST_FN(unsigned long, libc_strtoul, "strtoul", (const char* s, char** end, int base)) {
    return std::strtoul(s, end, base);
}
NX_GUEST_FN(long long, libc_strtoll, "strtoll", (const char* s, char** end, int base)) {
    return std::strtoll(s, end, base);
}
NX_GUEST_FN(unsigned long long, libc_strtoull, "strtoull", (const char* s, char** end, int base)) {
    return std::strtoull(s, end, base);
}
NX_GUEST_FN(double, libc_strtod, "strtod", (const char* s, char** end)) { return std::strtod(s, end); }
NX_GUEST_FN(float, libc_strtof, "strtof", (const char* s, char** end)) { return std::strtof(s, end); }
NX_GUEST_FN(int, libc_atoi, "atoi", (const char* s)) { return std::atoi(s); }
NX_GUEST_FN(long, libc_atol, "atol", (const char* s)) { return std::atol(s); }
