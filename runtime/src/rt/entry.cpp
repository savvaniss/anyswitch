// SPDX-License-Identifier: MIT
// Process start. The relinked main executable's entry stub jumps here with
// the initial stack pointer, its module info and the loader's fini routine.
//
// Start-up order follows https://switchbrew.org/wiki/Rtld as far as it is
// documented: all modules are mapped and relocated (done by the host loader),
// then module initialisers run, then nnMain. Initialiser order between
// subsdk modules and main is an assumption, recorded in docs/dev/debt/rt.md.
#include "nxrt/runtime.hpp"

#include <cstdlib>
#include <cstring>

extern "C" void nxrt_svc_dispatch(std::uint64_t* regs, std::uint32_t id, std::uint64_t pc);
extern "C" [[noreturn]] void nxrt_trap(const nxabi_module_info* info, std::uint32_t index);

namespace nxrt {
namespace {

int g_argc = 0;
char** g_argv = nullptr;
alignas(16) std::uint8_t g_main_thread_type[0x200];

using InitFn = void (*)();

void call_array(std::uintptr_t array, std::uint64_t count, bool reverse) {
    auto* fns = reinterpret_cast<InitFn*>(array);
    for (std::uint64_t i = 0; i < count; ++i) {
        InitFn fn = fns[reverse ? count - 1 - i : i];
        auto raw = reinterpret_cast<std::uintptr_t>(fn);
        if (raw != 0 && raw != ~std::uintptr_t(0)) {
            fn();
        }
    }
}

void prepare_module(const LoadedModule& m) {
    auto* rw = reinterpret_cast<nxabi_module_rw*>(m.at(m.info->rw_offset));
    init_tick_scale(rw->tick_mul_int, rw->tick_mul_frac);
    rw->svc_handler = reinterpret_cast<std::uint64_t>(&nxrt_svc_dispatch);
    rw->trap_handler = reinterpret_cast<std::uint64_t>(&nxrt_trap);
}

void run_initializers(const LoadedModule& m) {
    log("initialising %s", m.name.c_str());
    if (m.info->init_offset) {
        reinterpret_cast<InitFn>(m.at(m.info->init_offset))();
    }
    call_array(m.at(m.info->init_array_offset), m.info->init_array_count, false);
}

void finalize(const LoadedModule& m) {
    call_array(m.at(m.info->fini_array_offset), m.info->fini_array_count, true);
    if (m.info->fini_offset) {
        reinterpret_cast<InitFn>(m.at(m.info->fini_offset))();
    }
}

// Reverse of initialisation: main first, then subsdk modules in descending order.
void run_finalizers() {
    if (const LoadedModule* main = main_module()) {
        finalize(*main);
    }
    const auto& mods = modules();
    for (auto it = mods.rbegin(); it != mods.rend(); ++it) {
        if (!(it->info->flags & NXABI_MODULE_MAIN)) {
            finalize(*it);
        }
    }
}

} // namespace

int host_argc() { return g_argc; }
char** host_argv() { return g_argv; }

} // namespace nxrt

extern "C" __attribute__((visibility("default"), noreturn)) void
__nxrt_entry(std::uint64_t* initial_sp, const nxabi_module_info* main_info, void (*rtld_fini)()) {
    using namespace nxrt;
    g_argc = static_cast<int>(initial_sp[0]);
    g_argv = reinterpret_cast<char**>(initial_sp + 1);
    if (rtld_fini) {
        std::atexit(rtld_fini);
    }
    discover_modules();
    const LoadedModule* main = main_module();
    if (!main || main->info != main_info) {
        fatal(1, "main module info not found among loaded modules");
    }
    verify_tls_layout();
    install_crash_handlers();
    for (const auto& m : modules()) {
        prepare_module(m);
    }
    attach_guest_thread(g_main_thread_type);
    std::atexit(run_finalizers);

    for (const auto& m : modules()) {
        if (&m != main) {
            run_initializers(m);
        }
    }
    run_initializers(*main);

    if (main->info->nninit_startup_offset) {
        log("calling nninitStartup");
        reinterpret_cast<void (*)()>(main->at(main->info->nninit_startup_offset))();
    }
    if (!main->info->nnmain_offset) {
        fatal(1, "main module does not export nnMain");
    }
    log("calling nnMain");
    reinterpret_cast<void (*)()>(main->at(main->info->nnmain_offset))();
    log("nnMain returned");
    std::exit(0);
}
