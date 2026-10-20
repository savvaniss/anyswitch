// SPDX-License-Identifier: MIT
// main half of the two-module case: imports a function exported by subsdk0.
#include "guest/libc.h"

extern "C" int subsdk_add(int a, int b);

namespace {
struct MainInit {
    MainInit() { puts("main init"); }
} g_main_init;
} // namespace

extern "C" void nnMain() { printf("subsdk_add=%d\n", subsdk_add(2, 3)); }
