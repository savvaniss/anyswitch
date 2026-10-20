# relinker

Source of truth for formats: switchbrew NSO, NRO, MOD, NPDM, Rtld pages.

## Gaps

- **zstd-compressed NSO segments (flag bit 7, firmware 22.0.0+).** Console: supported. anyswitch: refused with a clear error. Fix: add a zstd decoder as a `3rdparty/` submodule.
- **Compressed NRO (firmware 23.0.0+).** Refused with a clear error. Needed only for runtime-loaded NRO plugins.
- **R_AARCH64_ABS32 and TLS relocations.** The host loader cannot apply them. anyswitch: conversion stops and names the relocation. Measure with `census` before deciding.
- **Text relocations.** Relocations inside `.text`/`.rodata` set `DT_TEXTREL`, which works with glibc but is slow and refused by some hardened systems. Not seen in practice yet.
- **16 KiB and 64 KiB page hosts.** Segments are 64 KiB aligned, but the original image's `.text`/`.rodata`/`.data` boundaries are 4 KiB aligned, so on 16 KiB kernels adjacent segments share pages. Needed for Apple silicon (macOS, Asahi) and some Android devices. Plan: relayout or merge permissions per host page (PLAN.md R3).
- **Fixed 39-bit placement.** Output is a PIE at whatever address the host picks. Titles that assume pointers fit in 36 or 39 bits will need a low fixed base (`--fixed-base`, not implemented).
- **Patch reach.** Stubs must be within ±128 MiB of every patched instruction. Modules whose text exceeds about 120 MiB will fail conversion with a branch-range error.
- **Data in `.text`.** The scanner treats every aligned word in `.text` as an instruction. A literal that happens to encode `svc`/`mrs` would be patched. Restricting the scan to `.eh_frame` FDE ranges would remove the risk.
- **Output formats.** Only AArch64 Linux ELF. Mach-O (macOS) and PE (Windows on ARM) writers are Phase 5.
- **x18.** Not measured. Windows on ARM and macOS reserve x18; if titles use it, those hosts need register rewriting (PLAN.md Q2).
