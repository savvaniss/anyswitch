// SPDX-License-Identifier: MIT
// Safety net for raw `svc` instructions in relinked code. Retail main modules
// are expected to reach the kernel only through the SDK, which the runtime
// replaces, so this handles the few SVCs that homebrew and inlined helpers use.
// Numbers and register conventions: https://switchbrew.org/wiki/SVC
// IPC (SendSyncRequest*) is deliberately unsupported: see AGENTS.md rule E3.
#include "nxrt/guest.hpp"
#include "nxrt/runtime.hpp"

#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sched.h>
#include <sys/mman.h>
#include <sys/random.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace nxrt {
namespace {

using guest::Result;

constexpr std::uint64_t kHeapReserve = std::uint64_t(4) << 30;
constexpr std::uint64_t kHeapGranule = 0x200000;

std::uint8_t* g_heap = nullptr;
std::uint64_t g_heap_size = 0;

const char* svc_name(std::uint32_t id) {
    static const char* const names[] = {
        nullptr, "SetHeapSize", "SetMemoryPermission", "SetMemoryAttribute", "MapMemory", "UnmapMemory",
        "QueryMemory", "ExitProcess", "CreateThread", "StartThread", "ExitThread", "SleepThread",
        "GetThreadPriority", "SetThreadPriority", "GetThreadCoreMask", "SetThreadCoreMask",
        "GetCurrentProcessorNumber", "SignalEvent", "ClearEvent", "MapSharedMemory", "UnmapSharedMemory",
        "CreateTransferMemory", "CloseHandle", "ResetSignal", "WaitSynchronization", "CancelSynchronization",
        "ArbitrateLock", "ArbitrateUnlock", "WaitProcessWideKeyAtomic", "SignalProcessWideKey", "GetSystemTick",
        "ConnectToNamedPort", "SendSyncRequestLight", "SendSyncRequest", "SendSyncRequestWithUserBuffer",
        "SendAsyncRequestWithUserBuffer", "GetProcessId", "GetThreadId", "Break", "OutputDebugString",
        "ReturnFromException", "GetInfo", "FlushEntireDataCache", "FlushDataCache", "MapPhysicalMemory",
        "UnmapPhysicalMemory", "GetDebugFutureThreadInfo", "GetLastThreadInfo", "GetResourceLimitLimitValue",
        "GetResourceLimitCurrentValue", "SetThreadActivity", "GetThreadContext3", "WaitForAddress",
        "SignalToAddress", "SynchronizePreemptionState",
    };
    if (id < sizeof(names) / sizeof(names[0]) && names[id]) {
        return names[id];
    }
    return "unknown";
}

Result set_heap_size(std::uint64_t size, std::uint64_t& address) {
    if (size % kHeapGranule != 0 || size > kHeapReserve) {
        return guest::kInvalidSize;
    }
    if (!g_heap) {
        void* p = mmap(nullptr, kHeapReserve, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        if (p == MAP_FAILED) {
            fatal(1, "cannot reserve guest heap: %s", std::strerror(errno));
        }
        g_heap = static_cast<std::uint8_t*>(p);
    }
    if (size > g_heap_size) {
        mprotect(g_heap + g_heap_size, size - g_heap_size, PROT_READ | PROT_WRITE);
    } else if (size < g_heap_size) {
        madvise(g_heap + size, g_heap_size - size, MADV_DONTNEED);
        mprotect(g_heap + size, g_heap_size - size, PROT_NONE);
    }
    g_heap_size = size;
    address = reinterpret_cast<std::uint64_t>(g_heap);
    return guest::kSuccess;
}

// MemoryInfo layout from switchbrew (QueryMemory).
struct MemoryInfo {
    std::uint64_t base;
    std::uint64_t size;
    std::uint32_t type;
    std::uint32_t attribute;
    std::uint32_t permission;
    std::uint32_t ipc_refcount;
    std::uint32_t device_refcount;
    std::uint32_t padding;
};
static_assert(sizeof(MemoryInfo) == 0x28);

void query_memory(MemoryInfo* out, std::uint64_t address) {
    std::memset(out, 0, sizeof *out);
    FILE* maps = std::fopen("/proc/self/maps", "r");
    std::uint64_t prev_end = 0;
    char line[512];
    while (maps && std::fgets(line, sizeof line, maps)) {
        unsigned long long lo = 0, hi = 0;
        char perms[5] = {};
        if (std::sscanf(line, "%llx-%llx %4s", &lo, &hi, perms) != 3) {
            continue;
        }
        if (address < lo) {
            out->base = prev_end;
            out->size = lo - prev_end;
            break;
        }
        if (address < hi) {
            out->base = lo;
            out->size = hi - lo;
            out->permission = (perms[0] == 'r' ? 1u : 0u) | (perms[1] == 'w' ? 2u : 0u) | (perms[2] == 'x' ? 4u : 0u);
            out->type = out->permission & 4 ? 3 /* Code */ : 5 /* Normal */;
            break;
        }
        prev_end = hi;
    }
    if (maps) {
        std::fclose(maps);
    }
    if (out->size == 0) {
        out->base = prev_end;
        out->size = ~prev_end + 1;
    }
}

Result get_info(std::uint32_t type, std::uint64_t subtype, std::uint64_t& value) {
    switch (type) {
    case 4: value = reinterpret_cast<std::uint64_t>(g_heap); return guest::kSuccess;
    case 5: value = kHeapReserve; return guest::kSuccess;
    case 6: value = kHeapReserve; return guest::kSuccess;
    case 7: value = g_heap_size; return guest::kSuccess;
    case 8: value = 0; return guest::kSuccess;
    case 11:
        if (subtype > 3) return guest::kInvalidEnumValue;
        if (getrandom(&value, sizeof value, 0) != sizeof value) value = guest_ticks_now();
        return guest::kSuccess;
    case 18: value = main_module() ? main_module()->info->program_id : 0; return guest::kSuccess;
    default: return guest::kInvalidEnumValue;
    }
}

} // namespace
} // namespace nxrt

extern "C" void nxrt_svc_dispatch(std::uint64_t* r, std::uint32_t id, std::uint64_t pc) {
    using namespace nxrt;
    log("svc %#x %s from %s", id, svc_name(id), describe_address(pc - 4).c_str());
    switch (id) {
    case 0x01: {
        std::uint64_t address = 0;
        r[0] = set_heap_size(r[1], address);
        r[1] = address;
        return;
    }
    case 0x06:
        query_memory(reinterpret_cast<MemoryInfo*>(r[0]), r[2]);
        r[0] = guest::kSuccess;
        r[1] = 0;
        return;
    case 0x07:
        std::fflush(stdout);
        std::exit(0);
    case 0x0B: {
        auto ns = static_cast<std::int64_t>(r[0]);
        if (ns <= 0) {
            sched_yield();
        } else {
            timespec ts{static_cast<time_t>(ns / 1000000000), static_cast<long>(ns % 1000000000)};
            while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {
            }
        }
        return;
    }
    case 0x1E: r[0] = guest_ticks_now(); return;
    case 0x25:
        r[0] = guest::kSuccess;
        r[1] = static_cast<std::uint64_t>(syscall(SYS_gettid));
        return;
    case 0x26:
        if (r[0] & 0x80000000u) {
            r[0] = guest::kSuccess;
            return;
        }
        fatal(kExitGuestCrash, "guest called svcBreak(reason %#" PRIx64 ", %#" PRIx64 ", %#" PRIx64 ") at %s", r[0],
              r[1], r[2], describe_address(pc - 4).c_str());
    case 0x27: {
        const char* text = reinterpret_cast<const char*>(r[0]);
        std::size_t len = r[1];
        while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\0')) {
            --len;
        }
        std::fflush(stdout);
        std::fprintf(stderr, "[guest] %.*s\n", static_cast<int>(len), text);
        r[0] = guest::kSuccess;
        return;
    }
    case 0x29: {
        std::uint64_t value = 0;
        r[0] = get_info(static_cast<std::uint32_t>(r[1]), r[3], value);
        r[1] = value;
        return;
    }
    default:
        fatal(kExitUnimplemented, "unimplemented SVC %#x (%s) called from %s", id, svc_name(id),
              describe_address(pc - 4).c_str());
    }
}
