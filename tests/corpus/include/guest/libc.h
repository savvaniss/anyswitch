// SPDX-License-Identifier: MIT
// The C library and C++ ABI surface corpus modules import. On a console these
// come from the title's `sdk` module (musl, libc++abi); under anyswitch they
// come from libnxrt.so. Prototypes follow C11/POSIX and the Itanium C++ ABI.
#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

extern "C" {
typedef struct GuestFile GuestFile;
extern GuestFile* stdin;
extern GuestFile* stdout;
extern GuestFile* stderr;

int printf(const char* fmt, ...);
int fprintf(GuestFile* f, const char* fmt, ...);
int sprintf(char* out, const char* fmt, ...);
int snprintf(char* out, size_t n, const char* fmt, ...);
int vprintf(const char* fmt, va_list ap);
int vfprintf(GuestFile* f, const char* fmt, va_list ap);
int vsprintf(char* out, const char* fmt, va_list ap);
int vsnprintf(char* out, size_t n, const char* fmt, va_list ap);
int puts(const char* s);
int putchar(int c);
int fputs(const char* s, GuestFile* f);
int fputc(int c, GuestFile* f);
size_t fwrite(const void* p, size_t size, size_t n, GuestFile* f);
int fflush(GuestFile* f);

void* malloc(size_t n);
void free(void* p);
void* calloc(size_t n, size_t size);
void* realloc(void* p, size_t n);
void* aligned_alloc(size_t align, size_t n);
void* memalign(size_t align, size_t n);
int posix_memalign(void** out, size_t align, size_t n);
size_t malloc_usable_size(void* p);

void* memcpy(void* d, const void* s, size_t n);
void* memmove(void* d, const void* s, size_t n);
void* memset(void* d, int c, size_t n);
int memcmp(const void* a, const void* b, size_t n);
void* memchr(const void* s, int c, size_t n);
size_t strlen(const char* s);
size_t strnlen(const char* s, size_t n);
int strcmp(const char* a, const char* b);
int strncmp(const char* a, const char* b, size_t n);
char* strcpy(char* d, const char* s);
char* strncpy(char* d, const char* s, size_t n);
char* strcat(char* d, const char* s);
char* strncat(char* d, const char* s, size_t n);
char* strchr(const char* s, int c);
char* strrchr(const char* s, int c);
char* strstr(const char* h, const char* n);
char* strdup(const char* s);
long strtol(const char* s, char** end, int base);
unsigned long strtoul(const char* s, char** end, int base);
long long strtoll(const char* s, char** end, int base);
unsigned long long strtoull(const char* s, char** end, int base);
double strtod(const char* s, char** end);
float strtof(const char* s, char** end);
int atoi(const char* s);
long atol(const char* s);

void abort(void);
void exit(int code);
void _Exit(int code);
int atexit(void (*fn)(void));
int* __errno_location(void);
int rand(void);
void srand(unsigned seed);
void qsort(void* base, size_t n, size_t size, int (*cmp)(const void*, const void*));

int __cxa_atexit(void (*fn)(void*), void* arg, void* dso);
int __cxa_guard_acquire(uint64_t* guard);
void __cxa_guard_release(uint64_t* guard);
void __cxa_guard_abort(uint64_t* guard);
void __cxa_pure_virtual(void);
void __cxa_deleted_virtual(void);
extern uintptr_t __stack_chk_guard;
void __stack_chk_fail(void);
}

namespace std {
struct nothrow_t {
    explicit nothrow_t() = default;
};
enum class align_val_t : size_t {};
} // namespace std

void* operator new(size_t n, const std::nothrow_t&) noexcept;
void* operator new[](size_t n, const std::nothrow_t&) noexcept;
void* operator new(size_t n, std::align_val_t align);
void operator delete(void* p, std::align_val_t align) noexcept;
void operator delete(void* p, size_t n) noexcept;
void operator delete[](void* p, size_t n) noexcept;
