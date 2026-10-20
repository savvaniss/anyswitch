// SPDX-License-Identifier: MIT
// Calls a function the runtime does not implement. The relinker binds it to a
// trap stub, and the runtime must stop with the function's name.
#include "guest/libc.h"
#include "nn/fs.hpp"

extern "C" void nnMain() {
    puts("before missing call");
    fflush(nullptr);
    static unsigned char cache[256];
    nn::Result r = nn::fs::MountRom("rom", cache, sizeof cache);
    printf("after missing call (should not print) %u\n", r.GetInnerValueForDebug());
}
