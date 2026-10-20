// SPDX-License-Identifier: MIT
// nn::os mutexes. Like threads, state lives in a side table keyed by the
// guest MutexType address, created on first use so statically initialised
// mutexes work without InitializeMutex. Every mutex is recursive here; see
// docs/dev/debt/nn_os.md.
#include "nxrt/guest.hpp"
#include "nxrt/runtime.hpp"

#include <memory>
#include <mutex>
#include <unordered_map>

namespace {

std::mutex g_table_lock;
std::unordered_map<void*, std::unique_ptr<std::recursive_mutex>> g_mutexes;

std::recursive_mutex& get(void* m) {
    std::lock_guard lock(g_table_lock);
    auto& slot = g_mutexes[m];
    if (!slot) {
        slot = std::make_unique<std::recursive_mutex>();
    }
    return *slot;
}

} // namespace

NX_GUEST_FN(void, os_InitializeMutex, "_ZN2nn2os15InitializeMutexEPNS0_9MutexTypeEbi",
            (void* mutex, bool /*recursive*/, int /*lock_level*/)) {
    get(mutex);
}

NX_GUEST_FN(void, os_FinalizeMutex, "_ZN2nn2os13FinalizeMutexEPNS0_9MutexTypeE", (void* mutex)) {
    std::lock_guard lock(g_table_lock);
    g_mutexes.erase(mutex);
}

NX_GUEST_FN(void, os_LockMutex, "_ZN2nn2os9LockMutexEPNS0_9MutexTypeE", (void* mutex)) { get(mutex).lock(); }

NX_GUEST_FN(bool, os_TryLockMutex, "_ZN2nn2os12TryLockMutexEPNS0_9MutexTypeE", (void* mutex)) {
    return get(mutex).try_lock();
}

NX_GUEST_FN(void, os_UnlockMutex, "_ZN2nn2os11UnlockMutexEPNS0_9MutexTypeE", (void* mutex)) {
    get(mutex).unlock();
}
