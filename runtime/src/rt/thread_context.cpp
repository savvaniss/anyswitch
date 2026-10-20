// SPDX-License-Identifier: MIT
// Per-thread guest state. Patched guest code reads tpidrro_el0 and tpidr_el0
// from the main executable's TLS block at tpidr_el0 + NXABI_TLS_TP_OFFSET.
// Thread Local Region layout: https://switchbrew.org/wiki/Thread_Local_Region
#include "nxrt/runtime.hpp"

#include <cstdlib>
#include <cstring>
#include <link.h>

namespace nxrt {
namespace {

struct Slots {
    void* tlr;
    void* guest_tp;
};

Slots* slots() {
    auto* tp = static_cast<std::uint8_t*>(__builtin_thread_pointer());
    return reinterpret_cast<Slots*>(tp + NXABI_TLS_TP_OFFSET);
}

int find_main_tls(struct dl_phdr_info* info, size_t, void* out) {
    for (int i = 0; i < info->dlpi_phnum; ++i) {
        if (info->dlpi_phdr[i].p_type == NXABI_PT_MODULE_INFO) {
            auto* mi = reinterpret_cast<const nxabi_module_info*>(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr);
            if (mi->flags & NXABI_MODULE_MAIN) {
                *static_cast<void**>(out) = info->dlpi_tls_data;
                return 1;
            }
        }
    }
    return 0;
}

} // namespace

void verify_tls_layout() {
    void* block = nullptr;
    dl_iterate_phdr(find_main_tls, &block);
    if (block != static_cast<void*>(slots())) {
        fatal(1, "TLS layout mismatch: main executable TLS block at %p, expected tp+%u = %p", block,
              NXABI_TLS_TP_OFFSET, static_cast<void*>(slots()));
    }
}

void attach_guest_thread(void* thread_type) {
    Slots* s = slots();
    if (s->tlr) {
        return;
    }
    void* tlr = std::aligned_alloc(NXABI_TLR_SIZE, NXABI_TLR_SIZE);
    if (!tlr) {
        fatal(1, "out of memory allocating a thread local region");
    }
    std::memset(tlr, 0, NXABI_TLR_SIZE);
    std::memcpy(static_cast<std::uint8_t*>(tlr) + NXABI_TLR_THREAD_TYPE_OFFSET, &thread_type, sizeof thread_type);
    s->tlr = tlr;
    s->guest_tp = nullptr;
}

void detach_guest_thread() {
    Slots* s = slots();
    std::free(s->tlr);
    s->tlr = nullptr;
    s->guest_tp = nullptr;
}

void* current_tlr() { return slots()->tlr; }

void* current_thread_type() {
    void* tlr = slots()->tlr;
    if (!tlr) {
        return nullptr;
    }
    void* t;
    std::memcpy(&t, static_cast<std::uint8_t*>(tlr) + NXABI_TLR_THREAD_TYPE_OFFSET, sizeof t);
    return t;
}

} // namespace nxrt
