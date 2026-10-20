// SPDX-License-Identifier: MIT
// nn::os threads on host pthreads. The guest's ThreadType storage is treated
// as opaque: state lives in a side table keyed by its address, so no guess
// about the console's ThreadType layout is written into guest memory.
// Known gaps (guest-provided stacks, priorities, core masks, names) are listed
// in docs/dev/debt/nn_os.md.
#include "nxrt/guest.hpp"
#include "nxrt/runtime.hpp"

#include <algorithm>
#include <memory>
#include <mutex>
#include <pthread.h>
#include <unordered_map>

namespace {

using nxrt::guest::Result;

struct HostThread {
    void* guest = nullptr;
    void (*entry)(void*) = nullptr;
    void* arg = nullptr;
    std::size_t stack_size = 0;
    int priority = 0;
    pthread_t handle{};
    bool started = false;
    bool joined = false;
};

std::mutex g_lock;
std::unordered_map<void*, std::unique_ptr<HostThread>> g_threads;

constexpr std::size_t kMinStack = 256 * 1024;

HostThread* lookup(void* guest, const char* caller) {
    std::lock_guard lock(g_lock);
    auto it = g_threads.find(guest);
    if (it == g_threads.end()) {
        nxrt::fatal(nxrt::kExitUnimplemented, "nn::os::%s on a ThreadType %p that CreateThread never saw", caller,
                    guest);
    }
    return it->second.get();
}

void* thread_main(void* p) {
    auto* t = static_cast<HostThread*>(p);
    nxrt::attach_guest_thread(t->guest);
    t->entry(t->arg);
    nxrt::detach_guest_thread();
    return nullptr;
}

Result create(void* guest, void (*entry)(void*), void* arg, std::size_t stack_size, int priority) {
    auto t = std::make_unique<HostThread>();
    t->guest = guest;
    t->entry = entry;
    t->arg = arg;
    t->stack_size = stack_size;
    t->priority = priority;
    std::lock_guard lock(g_lock);
    g_threads[guest] = std::move(t);
    return nxrt::guest::kSuccess;
}

} // namespace

NX_GUEST_FN(Result, os_CreateThread, "_ZN2nn2os12CreateThreadEPNS0_10ThreadTypeEPFvPvES3_S3_mi",
            (void* thread, void (*entry)(void*), void* arg, void* /*stack*/, std::size_t stack_size, int priority)) {
    return create(thread, entry, arg, stack_size, priority);
}

NX_GUEST_FN(Result, os_CreateThreadOnCore, "_ZN2nn2os12CreateThreadEPNS0_10ThreadTypeEPFvPvES3_S3_mii",
            (void* thread, void (*entry)(void*), void* arg, void* /*stack*/, std::size_t stack_size, int priority,
             int /*core*/)) {
    return create(thread, entry, arg, stack_size, priority);
}

NX_GUEST_FN(void, os_StartThread, "_ZN2nn2os11StartThreadEPNS0_10ThreadTypeE", (void* thread)) {
    HostThread* t = lookup(thread, "StartThread");
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, std::max(t->stack_size, kMinStack));
    int err = pthread_create(&t->handle, &attr, thread_main, t);
    pthread_attr_destroy(&attr);
    if (err != 0) {
        nxrt::fatal(1, "pthread_create failed (%d)", err);
    }
    t->started = true;
}

NX_GUEST_FN(void, os_WaitThread, "_ZN2nn2os10WaitThreadEPNS0_10ThreadTypeE", (void* thread)) {
    HostThread* t = lookup(thread, "WaitThread");
    if (t->started && !t->joined) {
        pthread_join(t->handle, nullptr);
        t->joined = true;
    }
}

NX_GUEST_FN(void, os_DestroyThread, "_ZN2nn2os13DestroyThreadEPNS0_10ThreadTypeE", (void* thread)) {
    HostThread* t = lookup(thread, "DestroyThread");
    if (t->started && !t->joined) {
        pthread_join(t->handle, nullptr);
    }
    std::lock_guard lock(g_lock);
    g_threads.erase(thread);
}

NX_GUEST_FN(void*, os_GetCurrentThread, "_ZN2nn2os16GetCurrentThreadEv", ()) {
    return nxrt::current_thread_type();
}
