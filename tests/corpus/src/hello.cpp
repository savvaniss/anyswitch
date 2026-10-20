// SPDX-License-Identifier: MIT
// Start-up, static initialisation and destruction, libc and C++ runtime imports.
#include "guest/libc.h"
#include "nn/os.hpp"

namespace {

int g_init_value = 0;

struct StaticProbe {
    StaticProbe() {
        g_init_value = 42;
        puts("static init ran");
    }
    ~StaticProbe() { puts("static fini ran"); }
};

StaticProbe g_probe;

int lazy_value() {
    static int value = g_init_value + 1; // guarded local static: __cxa_guard_*
    return value;
}

} // namespace

extern "C" void nnMain() {
    printf("hello from guest: %d\n", g_init_value);
    int argc = nn::os::GetHostArgc();
    char** argv = nn::os::GetHostArgv();
    printf("argc=%d argv[1]=%s\n", argc, argc > 1 ? argv[1] : "(none)");

    char buffer[64];
    snprintf(buffer, sizeof buffer, "%s-%d", "lazy", lazy_value());
    printf("%s len=%d\n", buffer, static_cast<int>(strlen(buffer)));

    int* numbers = new int[16];
    for (int i = 0; i < 16; ++i) numbers[i] = i * i;
    int* copy = static_cast<int*>(malloc(sizeof(int) * 16));
    memcpy(copy, numbers, sizeof(int) * 16);
    printf("heap %s\n", memcmp(copy, numbers, sizeof(int) * 16) == 0 && copy[15] == 225 ? "ok" : "BAD");
    free(copy);
    delete[] numbers;
    puts("nnMain done");
}
