// SPDX-License-Identifier: MIT
// Reports crashes in terms of guest modules (module+offset), which is what a
// bring-up report needs. Best effort: it runs inside a signal handler.
#include "nxrt/runtime.hpp"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ucontext.h>
#include <unistd.h>

namespace nxrt {
namespace {

const char* signal_name(int sig) {
    switch (sig) {
    case SIGSEGV: return "SIGSEGV";
    case SIGBUS: return "SIGBUS";
    case SIGILL: return "SIGILL";
    case SIGTRAP: return "SIGTRAP";
    case SIGFPE: return "SIGFPE";
    case SIGABRT: return "SIGABRT";
    default: return "signal";
    }
}

void on_crash(int sig, siginfo_t* si, void* raw) {
    auto* uc = static_cast<ucontext_t*>(raw);
    const auto& mc = uc->uc_mcontext;
    std::fflush(stdout);
    std::fprintf(stderr, "[nxrt] guest crash: %s (fault address %p) at %s\n", signal_name(sig), si->si_addr,
                 describe_address(mc.pc).c_str());
    std::fprintf(stderr, "[nxrt]   lr %s\n", describe_address(mc.regs[30]).c_str());
    for (int i = 0; i < 30; i += 2) {
        std::fprintf(stderr, "[nxrt]   x%-2d %016llx  x%-2d %016llx\n", i, static_cast<unsigned long long>(mc.regs[i]),
                     i + 1, static_cast<unsigned long long>(mc.regs[i + 1]));
    }
    std::fprintf(stderr, "[nxrt]   sp  %016llx  pc  %016llx\n", static_cast<unsigned long long>(mc.sp),
                 static_cast<unsigned long long>(mc.pc));
    std::fflush(stderr);
    _exit(kExitGuestCrash);
}

} // namespace

void install_crash_handlers() {
    const char* keep = std::getenv("NXRT_NO_CRASH_HANDLER");
    if (keep && *keep && *keep != '0') {
        return;
    }
    struct sigaction sa;
    std::memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_crash;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    for (int sig : {SIGSEGV, SIGBUS, SIGILL, SIGTRAP, SIGFPE}) {
        sigaction(sig, &sa, nullptr);
    }
}

} // namespace nxrt
