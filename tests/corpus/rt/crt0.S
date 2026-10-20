// SPDX-License-Identifier: MIT
// Module start for the synthetic corpus. Layouts of ModuleHeaderLocation and
// MOD0 follow https://switchbrew.org/wiki/MOD. The first 0x80 bytes are kept
// free so the same image can also be written as an NRO (its header lives at
// 0x10..0x80 of .text, https://switchbrew.org/wiki/NRO).

    .section .text.nx_start, "ax", %progbits
    .global __nx_module_start
    .hidden __nx_module_start
    .type __nx_module_start, %function
__nx_module_start:
    .word 0
    .word __nx_mod0 - __nx_module_start
    .space 0x78

    .hidden _DYNAMIC
    .hidden __bss_start__
    .hidden __bss_end__
    .hidden __eh_frame_hdr_start
    .hidden __eh_frame_hdr_end

    .section .rodata.nx_mod0, "a", %progbits
    .balign 4
__nx_mod0:
    .ascii "MOD0"
    .word _DYNAMIC - __nx_mod0
    .word __bss_start__ - __nx_mod0
    .word __bss_end__ - __nx_mod0
    .word __eh_frame_hdr_start - __nx_mod0
    .word __eh_frame_hdr_end - __nx_mod0
    .word __nx_module_runtime - __nx_mod0

    .section .bss.nx_module_runtime, "aw", %nobits
    .balign 8
__nx_module_runtime:
    .space 0xD0
