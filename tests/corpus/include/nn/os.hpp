// SPDX-License-Identifier: MIT
// Clean-room declarations of the nn::os functions the runtime implements.
// Names and parameter lists follow public symbol reconstructions
// (open-ead/nnheaders, nnsdk-rs); storage types are opaque blocks of the
// publicly documented sizes. These prototypes double as the mangling check
// for runtime/src/nn_os: a mismatch shows up as a trapped import.
#pragma once

namespace nn {

class Result {
public:
    bool IsSuccess() const { return value_ == 0; }
    unsigned GetInnerValueForDebug() const { return value_; }

private:
    unsigned value_;
};

class TimeSpan {
public:
    constexpr TimeSpan() = default;
    static constexpr TimeSpan FromNanoSeconds(long long ns) { return TimeSpan(ns); }
    static constexpr TimeSpan FromMilliSeconds(long long ms) { return TimeSpan(ms * 1000000); }
    constexpr long long GetNanoSeconds() const { return ns_; }

private:
    constexpr explicit TimeSpan(long long ns) : ns_(ns) {}
    long long ns_ = 0;
};

namespace os {

class Tick {
public:
    constexpr Tick() = default;
    constexpr explicit Tick(long long value) : value_(value) {}
    constexpr long long GetInt64Value() const { return value_; }

private:
    long long value_ = 0;
};

struct ThreadType {
    alignas(16) unsigned char storage[0x1C0];
};

struct MutexType {
    alignas(8) unsigned char storage[0x20];
};

using ThreadFunction = void (*)(void*);

Tick GetSystemTick();
long long GetSystemTickFrequency();
TimeSpan ConvertToTimeSpan(Tick tick);
void SleepThread(TimeSpan time);
void YieldThread();

int GetHostArgc();
char** GetHostArgv();

Result CreateThread(ThreadType* thread, ThreadFunction function, void* argument, void* stack,
                    unsigned long stack_size, int priority);
Result CreateThread(ThreadType* thread, ThreadFunction function, void* argument, void* stack,
                    unsigned long stack_size, int priority, int ideal_core);
void StartThread(ThreadType* thread);
void WaitThread(ThreadType* thread);
void DestroyThread(ThreadType* thread);
ThreadType* GetCurrentThread();

void InitializeMutex(MutexType* mutex, bool recursive, int lock_level);
void FinalizeMutex(MutexType* mutex);
void LockMutex(MutexType* mutex);
bool TryLockMutex(MutexType* mutex);
void UnlockMutex(MutexType* mutex);

} // namespace os
} // namespace nn
