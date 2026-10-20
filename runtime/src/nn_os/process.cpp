// SPDX-License-Identifier: MIT
#include "nxrt/guest.hpp"
#include "nxrt/runtime.hpp"

NX_GUEST_FN(int, os_GetHostArgc, "_ZN2nn2os11GetHostArgcEv", ()) { return nxrt::host_argc(); }

NX_GUEST_FN(char**, os_GetHostArgv, "_ZN2nn2os11GetHostArgvEv", ()) { return nxrt::host_argv(); }
