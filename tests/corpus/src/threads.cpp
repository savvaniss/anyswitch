// SPDX-License-Identifier: MIT
// nn::os threads and mutexes, per-thread Thread Local Regions, and atomics
// (which compile to ldxr/stxr loops on Armv8.0, like Switch code).
#include "guest/libc.h"
#include "nn/os.hpp"

namespace {

constexpr int kThreads = 4;
constexpr int kIterations = 20000;

nn::os::MutexType g_mutex;
long g_locked_counter = 0;
long g_atomic_counter = 0;

struct Worker {
    nn::os::ThreadType thread;
    unsigned long tlr = 0;
    bool identity_ok = false;
};

Worker g_workers[kThreads];

unsigned long read_tlr() {
    unsigned long v;
    asm volatile("mrs %0, tpidrro_el0" : "=r"(v));
    return v;
}

void work(void* arg) {
    auto* w = static_cast<Worker*>(arg);
    w->tlr = read_tlr();
    void* from_tlr = nullptr;
    memcpy(&from_tlr, reinterpret_cast<unsigned char*>(w->tlr) + 0x1F8, sizeof from_tlr);
    w->identity_ok = nn::os::GetCurrentThread() == &w->thread && from_tlr == &w->thread;
    for (int i = 0; i < kIterations; ++i) {
        nn::os::LockMutex(&g_mutex);
        ++g_locked_counter;
        nn::os::UnlockMutex(&g_mutex);
        __atomic_fetch_add(&g_atomic_counter, 1, __ATOMIC_RELAXED);
    }
}

alignas(4096) unsigned char g_stacks[kThreads][0x4000];

} // namespace

extern "C" void nnMain() {
    nn::os::InitializeMutex(&g_mutex, false, 0);
    for (int i = 0; i < kThreads; ++i) {
        nn::Result r = i % 2 ? nn::os::CreateThread(&g_workers[i].thread, work, &g_workers[i], g_stacks[i],
                                                    sizeof g_stacks[i], 44)
                             : nn::os::CreateThread(&g_workers[i].thread, work, &g_workers[i], g_stacks[i],
                                                    sizeof g_stacks[i], 44, 0);
        if (!r.IsSuccess()) printf("CreateThread failed %#x\n", r.GetInnerValueForDebug());
    }
    for (auto& w : g_workers) nn::os::StartThread(&w.thread);
    for (auto& w : g_workers) nn::os::WaitThread(&w.thread);
    for (auto& w : g_workers) nn::os::DestroyThread(&w.thread);
    nn::os::FinalizeMutex(&g_mutex);

    bool distinct = true;
    for (int i = 0; i < kThreads; ++i) {
        distinct &= g_workers[i].identity_ok && g_workers[i].tlr != 0 && g_workers[i].tlr != read_tlr();
        for (int j = 0; j < i; ++j) distinct &= g_workers[i].tlr != g_workers[j].tlr;
    }
    printf("locked counter %ld\n", g_locked_counter);
    printf("atomic counter %ld\n", g_atomic_counter);
    printf("threads %s\n", distinct ? "distinct ok" : "BAD");
}
