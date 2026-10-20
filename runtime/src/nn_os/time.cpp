// SPDX-License-Identifier: MIT
// nn::os time functions. Names from public symbol reconstructions
// (open-ead/nnheaders, nnsdk-rs); semantics from switchbrew (19.2 MHz tick).
#include "nxrt/guest.hpp"
#include "nxrt/runtime.hpp"

#include <cerrno>
#include <ctime>
#include <sched.h>

using nxrt::guest::Tick;
using nxrt::guest::TimeSpan;

NX_GUEST_FN(Tick, os_GetSystemTick, "_ZN2nn2os13GetSystemTickEv", ()) {
    return Tick{static_cast<std::int64_t>(nxrt::guest_ticks_now())};
}

NX_GUEST_FN(std::int64_t, os_GetSystemTickFrequency, "_ZN2nn2os22GetSystemTickFrequencyEv", ()) {
    return NXABI_GUEST_TICK_HZ;
}

NX_GUEST_FN(TimeSpan, os_ConvertToTimeSpan, "_ZN2nn2os17ConvertToTimeSpanENS0_4TickE", (Tick tick)) {
    return TimeSpan{nxrt::ticks_to_ns(tick.value)};
}

NX_GUEST_FN(void, os_SleepThread, "_ZN2nn2os11SleepThreadENS_8TimeSpanE", (TimeSpan span)) {
    if (span.ns <= 0) {
        sched_yield();
        return;
    }
    timespec ts{static_cast<time_t>(span.ns / 1000000000), static_cast<long>(span.ns % 1000000000)};
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {
    }
}

NX_GUEST_FN(void, os_YieldThread, "_ZN2nn2os11YieldThreadEv", ()) { sched_yield(); }
