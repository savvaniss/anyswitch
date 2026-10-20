/* SPDX-License-Identifier: MIT */
/*
 * The contract between the relinker (host tool) and the runtime (libnxrt.so).
 * Every relinked module carries an nxabi_module_info record, found through a
 * program header of type NXABI_PT_MODULE_INFO. Any change to this file that
 * changes layout or meaning must bump NXABI_VERSION and update
 * docs/dev/ARCHITECTURE.md ("Relinker/runtime contract").
 */
#ifndef NXABI_NXABI_H
#define NXABI_NXABI_H

#include <stdint.h>

#define NXABI_VERSION 1u
#define NXABI_MODULE_INFO_MAGIC 0x494D584Eu /* "NXMI" */
#define NXABI_PT_MODULE_INFO 0x6E780001u    /* inside PT_LOOS..PT_HIOS */

/* Every dynamic symbol name of a relinked guest module gets this prefix, so guest
 * names (malloc, operator new, nn::...) never interpose host libraries. */
#define NXABI_SYMBOL_PREFIX "nx$"

/* Unprefixed symbol the main module's entry stub jumps to. */
#define NXABI_ENTRY_SYMBOL "__nxrt_entry"

/* The main executable gets a PT_TLS block of NXABI_TLS_SIZE bytes aligned to
 * NXABI_TLS_ALIGN. On AArch64 Linux (glibc, musl, bionic) the executable's TLS
 * block then starts at tpidr_el0 + NXABI_TLS_TP_OFFSET. Patched guest code reads
 * the slots below through that fixed offset. */
#define NXABI_TLS_ALIGN 64u
#define NXABI_TLS_SIZE 64u
#define NXABI_TLS_TP_OFFSET 64u
#define NXABI_TLS_SLOT_TLR 0u      /* value the guest sees in tpidrro_el0 */
#define NXABI_TLS_SLOT_GUEST_TP 8u /* value the guest sees in tpidr_el0 */

#define NXABI_GUEST_TICK_HZ 19200000u
#define NXABI_TLR_SIZE 0x200u
#define NXABI_TLR_THREAD_TYPE_OFFSET 0x1F8u

enum nxabi_module_flags {
    NXABI_MODULE_MAIN = 1u << 0,
    NXABI_MODULE_PATCHED = 1u << 1,
    NXABI_MODULE_ORDERED_EXCLUSIVES = 1u << 2,
};

enum nxabi_address_space {
    NXABI_AS_32BIT = 0,
    NXABI_AS_64BIT_36 = 1,
    NXABI_AS_32BIT_NO_RESERVED = 2,
    NXABI_AS_64BIT_39 = 3,
    NXABI_AS_64BIT_42 = 4,
    NXABI_AS_UNKNOWN = 0xFFFFFFFFu,
};

/* Writable per-module block, filled by the runtime before any guest code runs. */
typedef struct nxabi_module_rw {
    uint64_t tick_mul_int;  /* guest_ticks = host_cntvct * tick_mul_int + umulh(host_cntvct, tick_mul_frac) */
    uint64_t tick_mul_frac;
    uint64_t svc_handler;   /* void (*)(uint64_t regs[8], uint32_t svc_id, uint64_t return_pc) */
    uint64_t trap_handler;  /* void (*)(const nxabi_module_info*, uint32_t import_index), never returns */
    uint64_t runtime_cookie;
    uint64_t reserved[3];
} nxabi_module_rw;

#define NXABI_RW_TICK_MUL_INT_OFFSET 0u
#define NXABI_RW_SVC_HANDLER_OFFSET 16u
#define NXABI_RW_TRAP_HANDLER_OFFSET 24u

/* All *_offset fields are relative to the ELF load bias (the runtime address of
 * vaddr 0). A zero offset means "absent"; vaddr 0 always holds the ELF header. */
typedef struct nxabi_module_info {
    uint32_t magic;
    uint32_t abi_version;
    uint32_t flags;
    uint32_t load_order; /* exefs order: main = 0, subsdk0 = 1, ... subsdk9 = 10 */
    char name[32];
    uint8_t module_id[32];

    uint64_t image_offset; /* where the original module's address 0 now lives */
    uint64_t image_size;   /* original text..bss end, page aligned */
    uint64_t text_offset, text_size;
    uint64_t ro_offset, ro_size;
    uint64_t data_offset, data_size;
    uint64_t bss_offset, bss_size;
    uint64_t mod0_offset;
    uint64_t eh_frame_hdr_offset, eh_frame_hdr_size;

    uint64_t init_offset, fini_offset;
    uint64_t init_array_offset, init_array_count;
    uint64_t fini_array_offset, fini_array_count;
    uint64_t nnmain_offset;
    uint64_t nninit_startup_offset;

    uint64_t rw_offset;         /* nxabi_module_rw */
    uint64_t trap_names_offset; /* uint32_t name_offsets[trap_count], then NUL-terminated names */
    uint32_t trap_count;
    uint32_t patch_count;

    uint64_t npdm_offset, npdm_size; /* verbatim main.npdm, main module only */
    uint32_t address_space;          /* enum nxabi_address_space */
    uint32_t main_thread_priority;
    uint32_t main_thread_core;
    uint32_t main_thread_stack_size;
    uint64_t program_id;
    uint64_t reserved[4];
} nxabi_module_info;

#ifdef __cplusplus
static_assert(sizeof(nxabi_module_rw) == 64, "nxabi_module_rw layout");
static_assert(sizeof(nxabi_module_info) == 344, "nxabi_module_info layout");
#endif

#endif
