# Architecture

This describes what is implemented. The longer-term design and its reasoning are in [../PLAN.md](../PLAN.md) and [../FEASIBILITY.md](../FEASIBILITY.md).

## Overview

```
exefs/ main, main.npdm, subsdk0..9   (rtld and sdk are ignored: the runtime replaces them)
        |
        v  anyswitch-relink exefs
out/main                 AArch64 Linux PIE, entry stub -> __nxrt_entry
out/lib/subsdkN.so       AArch64 Linux shared objects
out/lib/libnxrt.so       the runtime (copied in by the user or the packager)
        |
        v  ld.so maps and relocates everything, binding imports by name
__nxrt_entry -> discover modules -> init threads/TLS -> run initialisers -> nnMain
```

Guest machine code runs unchanged except for the patch classes below. There is no interpreter, no JIT and no IPC emulation.

## Components

| Component | Path | Role |
|---|---|---|
| Format library | `core/format` | Parses NSO (LZ4, SHA-256 checks), NRO, NPDM, MOD0, the dynamic section, symbols and relocations (RELA, REL, RELR, PLT) into one `nx::Module`. Refuses packaged input. |
| Patch scanner | `core/relinker/src/patcher.cpp` | Finds instructions that cannot run unchanged in a host process. |
| Census | `core/relinker/src/census.cpp` | Counts privileged instructions and relocation types (Phase 0 measurement). |
| Writer | `core/relinker/src/writer.cpp` | Lays out and writes the host ELF, emits stubs, rewrites relocations and symbols. |
| CLI | `core/relinker/main.cpp` | `info`, `symbols`, `census`, `audit`, `provided`, `module`, `exefs`. |
| Runtime core | `runtime/src/rt` | Entry, module discovery, guest thread state, SVC safety net, import traps, crash reports, time. |
| Guest APIs | `runtime/src/<module>` | Native implementations of SDK functions, exported as `nx$<symbol>`. |

## Output layout

Every `PT_LOAD` segment uses file offset == virtual address and 64 KiB alignment, so the file maps correctly on 4, 16 and 64 KiB kernels as far as segment placement goes (permissions within a 4 KiB-granular original image still assume 4 KiB pages, see the debt file).

| Address | Perm | Contents |
|---|---|---|
| `0` | R | ELF header, program headers, `.interp` (main), `.dynsym`, `.dynstr`, `.hash`, `.rela.dyn`, `.rela.plt`, module info, trap names, `main.npdm` copy |
| `B` | RX | stubs: entry stub (main), shared SVC/tick/trap handlers, one stub per patched instruction, one stub per trapped import |
| `H` | as original | the original module image: `.text` RX, `.rodata` R, `.data` + `.bss` RW |
| `C` | RW | `.dynamic`, the entry GOT slot (main), `nxabi_module_rw`, then the TLS block (main) |

The original image stays contiguous, so every PC-relative reference inside it remains valid. Only absolute values move by `H`: `RELATIVE` addends, defined symbol values, init/fini array addresses and the module-info offsets.

## Symbol naming

Every dynamic symbol name of a converted module gets the prefix `nx$`. Guest names such as `malloc`, `operator new` or `nn::os::LockMutex` therefore never interpose host libraries, and host code never binds to guest definitions. The runtime exports `nx$<exact mangled name>` through `NX_GUEST_FN` / `NX_GUEST_VAR` (`runtime/include/nxrt/guest.hpp`), and its linker version script (`runtime/nxrt.map`) hides everything else. Imports between game modules (main importing from subsdk0) resolve through the same prefixed names.

Converted modules set `DT_BIND_NOW` and `DF_1_NOW`: Switch PLT stubs are never used lazily.

## Missing imports

With `--registry <libnxrt.so>`, the relinker knows which guest symbols the runtime provides.

- A missing import that is called through the PLT (it has a `JUMP_SLOT` relocation) is bound to a trap stub. Calling it stops the process with exit status 86 and the demangled name, for example `unimplemented guest function nn::fs::MountRom(char const*, void*, unsigned long)`.
- A missing import used only as data (`GLOB_DAT`/`ABS64`, no PLT call) stays symbolic, so the host loader refuses to start and names it. It is listed in `relink-report.json` under `unresolved_data`.
- Weak imports stay symbolic and resolve to 0 when absent.

## Patch classes

| Class | Instruction | Handling | Why |
|---|---|---|---|
| `svc` | `svc #imm` | stub saves x16/x17/lr, calls the shared handler, which saves x0-x7, x18, lr and NZCV and calls `svc_handler(regs, imm, pc)` | the host kernel would treat it as a Linux syscall |
| `mrs_tpidrro_el0` | `mrs xN, tpidrro_el0` | `mrs xN, tpidr_el0; ldr xN, [xN, #64]` | the guest's Thread Local Region pointer lives in a TLS slot; user space cannot set `tpidrro_el0` |
| `mrs_tpidr_el0` / `msr_tpidr_el0` | guest thread pointer | load/store slot at `tp + 72` | `tpidr_el0` belongs to the host C library |
| `mrs_counter` | `mrs xN, cntpct_el0` / `cntvct_el0` | call shared helper: host `cntvct_el0` scaled to 19.2 MHz | host counters run at other rates |
| `mrs_cntfrq_el0` | `mrs xN, cntfrq_el0` | constant 19200000 | same |
| `exclusive_to_ordered` | `ldxr/stxr` family | rewritten in place to `ldaxr/stlxr` | Cortex-A57 behaved as if exclusives were ordered; newer cores do not (lesson from yuzu NCE, documented publicly) |
| `nop` | the above reading into `xzr` | rewritten in place to `nop` | no effect on the guest |

Each patched instruction becomes a `B` to its stub, and the stub ends with a `B` back to the next instruction. Stubs live within ±128 MiB of the code.

## Relinker/runtime contract

Defined in `core/abi/include/nxabi/nxabi.h`, version `NXABI_VERSION` (currently 1).

- **Module info.** Every converted module has a program header of type `NXABI_PT_MODULE_INFO` (0x6e780001) pointing at an `nxabi_module_info` record: name, load order, module ID, segment offsets, MOD0, `.eh_frame_hdr`, init/fini functions and arrays, `nnMain`/`nninitStartup`, the RW block, trap names, NPDM fields. All offsets are relative to the load bias; zero means absent.
- **RW block.** `nxabi_module_rw` holds the tick scale and the addresses of the runtime's SVC and trap handlers. The runtime fills it for every module before any guest code runs.
- **TLS.** The main executable has a 64-byte `PT_TLS` block aligned to 64. On AArch64 Linux (glibc, musl and bionic all agree) it sits at `tpidr_el0 + 64`. Slot 0 is the guest TLR pointer, slot 8 the guest thread pointer. The runtime checks this at start-up with `dl_iterate_phdr` and stops if it does not hold.
- **Entry.** The main executable's entry stub calls `__nxrt_entry(initial_sp, main_module_info, rtld_fini)`.

Changing any of this means bumping `NXABI_VERSION` (rule E2 in AGENTS.md). The runtime refuses modules relinked for another version.

## Runtime start-up

1. `ld.so` maps the executable, `subsdkN.so` and `libnxrt.so`, applies relocations and runs `libnxrt.so`'s own constructors.
2. `__nxrt_entry` records argc/argv, finds every module through `dl_iterate_phdr`, verifies the TLS layout, installs crash handlers and fills each module's RW block.
3. The main thread gets a Thread Local Region (0x200 bytes, 0x200-aligned) whose `ThreadType` field points at a runtime-owned block.
4. Module initialisers run: subsdk modules in load order, then main (`DT_INIT`, then `DT_INIT_ARRAY`). The relinker strips these from the dynamic section so the host loader does not run guest code before the runtime is ready.
5. `nninitStartup` (if exported), then `nnMain`. When `nnMain` returns the process exits; finalisers run in reverse order.

## Guest threads

Every host thread that runs guest code needs `attach_guest_thread()` first: it allocates the TLR and fills the TLS slots. `nn::os::StartThread` does this for guest threads. Callbacks from host libraries on host-created threads (audio, input) must do the same before calling into guest code.

## Diagnostics

- `NXRT_LOG=1` prints module discovery, tick scale, SVCs and start-up steps.
- Crashes print the signal, the faulting location as `module+offset`, `lr` and all registers, then exit with status 87.
- `NXRT_NO_CRASH_HANDLER=1` leaves signals to the debugger.
- Converted files keep a `.symtab` with original names, so `gdb` and `objdump` show guest function names.
