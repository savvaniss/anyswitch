// SPDX-License-Identifier: MIT
// subsdk0 half of the two-module case: exports a function to main and must be
// initialised before main.
#include "guest/libc.h"

namespace {
struct SubInit {
    SubInit() { puts("subsdk0 init"); }
} g_sub_init;
} // namespace

extern "C" __attribute__((visibility("default"))) int subsdk_add(int a, int b) { return a + b; }
