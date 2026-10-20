// SPDX-License-Identifier: MIT
// Runtime-internal interfaces shared by the rt/ core and the guest API modules.
#pragma once

#include "nxabi/nxabi.h"

#include <cstdarg>
#include <cstdint>
#include <string>
#include <vector>

namespace nxrt {

// Exit status used when guest code reaches something the runtime does not
// implement (trapped import, unimplemented SVC, NX_UNIMPLEMENTED).
constexpr int kExitUnimplemented = 86;
// Exit status for guest crashes reported by the signal handler.
constexpr int kExitGuestCrash = 87;

struct LoadedModule {
    const nxabi_module_info* info = nullptr;
    std::uintptr_t base = 0;
    std::string name;

    std::uintptr_t at(std::uint64_t offset) const { return offset ? base + offset : 0; }
    bool contains_image(std::uintptr_t address) const {
        return address >= base + info->image_offset && address < base + info->image_offset + info->image_size;
    }
};

// ---- modules (rt/modules.cpp) ----
void discover_modules();
const std::vector<LoadedModule>& modules();
const LoadedModule* main_module();
const LoadedModule* module_for_address(std::uintptr_t address);
const LoadedModule* module_for_info(const nxabi_module_info* info);
std::string describe_address(std::uintptr_t address);

// ---- guest thread context (rt/thread_context.cpp) ----
// Every host thread that runs guest code needs a Thread Local Region and the
// two TLS slots the patched code reads. thread_type is what nn::os reports as
// the current ThreadType (TLR + 0x1F8).
void attach_guest_thread(void* thread_type);
void detach_guest_thread();
void* current_tlr();
void* current_thread_type();
void verify_tls_layout();

// ---- time (rt/time.cpp) ----
void init_tick_scale(std::uint64_t& mul_int, std::uint64_t& mul_frac);
std::uint64_t guest_ticks_now();
std::int64_t ticks_to_ns(std::int64_t ticks);

// ---- process (rt/entry.cpp) ----
int host_argc();
char** host_argv();

// ---- diagnostics (rt/log.cpp) ----
bool log_enabled();
void log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
[[noreturn]] void fatal(int exit_code, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
std::string demangle(const char* name);
void install_crash_handlers();

} // namespace nxrt

// Fail loudly when guest code reaches a path the runtime does not support yet.
// Every use must have an entry in docs/dev/debt/<module>.md.
#define NX_UNIMPLEMENTED(what) \
    ::nxrt::fatal(::nxrt::kExitUnimplemented, "unimplemented: %s (%s:%d)", what, __FILE__, __LINE__)
