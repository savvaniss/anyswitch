/* SPDX-License-Identifier: MIT
 * Links a synthetic test module the way Switch executables are laid out:
 * .text at 0, then page-aligned .rodata and .data, three load segments,
 * dynamic linking by symbol name with a SysV hash table. */
OUTPUT_FORMAT(elf64-littleaarch64)
OUTPUT_ARCH(aarch64)
ENTRY(__nx_module_start)

PHDRS
{
  text PT_LOAD FLAGS(5);
  ro   PT_LOAD FLAGS(4);
  data PT_LOAD FLAGS(6);
  dyn  PT_DYNAMIC;
}

SECTIONS
{
  . = 0;
  .text : {
    KEEP(*(.text.nx_start))
    *(.text .text.*)
  } :text
  .plt : { *(.plt .plt.*) } :text
  .iplt : { *(.iplt) } :text

  . = ALIGN(0x1000);
  .rodata : {
    *(.rodata .rodata.*)
  } :ro
  .note.gnu.build-id : { *(.note.gnu.build-id) } :ro
  .dynsym : { *(.dynsym) } :ro
  .dynstr : { *(.dynstr) } :ro
  .hash : { *(.hash) } :ro
  .gnu.hash : { *(.gnu.hash) } :ro
  .rela.dyn : { *(.rela.dyn) } :ro
  .rela.plt : { *(.rela.plt) } :ro
  .eh_frame_hdr : {
    __eh_frame_hdr_start = .;
    *(.eh_frame_hdr)
    __eh_frame_hdr_end = .;
  } :ro
  .eh_frame : { KEEP(*(.eh_frame)) } :ro

  . = ALIGN(0x1000);
  .data : { *(.data .data.*) } :data
  .init_array : {
    KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
    KEEP(*(.init_array))
  } :data
  .fini_array : {
    KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
    KEEP(*(.fini_array))
  } :data
  .dynamic : { *(.dynamic) } :data :dyn
  .got : { *(.got) } :data
  .got.plt : { *(.got.plt) } :data
  .bss (NOLOAD) : {
    __bss_start__ = .;
    *(.bss .bss.*)
    *(COMMON)
    . = ALIGN(8);
    __bss_end__ = .;
  } :data

  /DISCARD/ : { *(.comment) *(.note.GNU-stack) *(.interp) }
}
