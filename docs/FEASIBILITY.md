# AnySwitch — Feasibility study

*An AnyPS5-style static relinker for Nintendo Switch (and, later, Switch 2) executables.*
*Research date: 2026-10-20. Companion document: [PLAN.md](PLAN.md).*

---

## 0. Verdict in one page

| Question | Answer |
|---|---|
| Can the AnyPS5 approach (relink the game, reimplement the system libraries natively, recompile shaders, **no emulator**) be applied to Switch 1 games? | **Yes, with one structural change.** PS5 games are x86-64 and run unchanged on a PC. Switch games are AArch64. On **ARM64 hosts** (Linux ARM64, Apple Silicon, Windows on ARM, Android) the game code can run natively in-process after a small instruction-patching pass, which is exactly the AnyPS5 model. On **x86-64 hosts** a **static recompilation** stage (AArch64 to C/LLVM to native) must be inserted. Both are proven in public projects as of October 2026. |
| Is the library-replacement model valid for Switch? | **Yes, and it is cleaner than on PS5.** Retail games are dynamically linked against one shared library, `sdk` (nnSdk), by *symbol name*. A measured sample (`Hades`) has 631 imports in `main`, all by name: libc, libc++, every `nn::*` family, and NVN. The Switch has no NID hashing to reverse. |
| Is the graphics side tractable? | **Yes.** NVN is a thin, object-based API that games bind by name through `nvnBootstrapLoader`/`nvnDeviceGetProcAddress`. Intercepting at the API level on Vulkan has been done by three 2026 projects. Shaders ship as precompiled Maxwell SASS, so a SASS-to-SPIR-V recompiler is mandatory; two open implementations exist (one MIT, one GPL). |
| Switch 2? | **Not possible to start today.** As of 2026-10-20 there is no public kernel exploit, CFW, modchip, key dump, or decrypted Switch 2 title. The executable format of Switch 2 titles, the NVN successor API, and the Ampere SASS variant are all unverified publicly. The plan treats Switch 2 as a design constraint (keep interfaces ISA- and GPU-pluggable), not a deliverable. |
| Legal risk vs AnyPS5? | **Materially higher.** Sony has not acted against shadPS4 or AnyPS5. Nintendo has two settlements ($2.4M yuzu, $2M Modded Hardware), a $4.5M default judgment, and removed roughly 420 emulator repositories from GitHub in 2026. Its legal theory ("primarily designed to circumvent", prod.keys) applies verbatim to any tool that touches keys or decryption. Mitigation is a design constraint: **accept only already-decrypted files, never handle keys, ship no assets, publish no dumping instructions.** No static-recompilation project has been taken down to date. |
| Scale? | AnyPS5 is ~390k lines of library code (352 libraries), a 71k-line shader recompiler, a 20k-line relinker, ~1,000 tests, and reached one verified playable title about two months after going public, with thousands of PRs. A Switch equivalent is the same order of magnitude. Reaching a first retail frame on one ARM64 host is a 1–2 person, 6–12 month effort; the x86-64 recompiler and host breadth roughly double that. |

---

## 1. What AnyPS5 actually is

AnyPS5 (github.com/boykopovar/AnyPS5, public since 2026-08-03, GPL-2.0-only) is **not** an emulator and not a controller tool. It is three things:

1. **A relinker** (`core/relinker`, ~20k LOC): takes a *decrypted* PS5 ELF, reads its imports (Sony NIDs, i.e. hashed symbol names), optionally lowers AMD-only x86 instructions for Intel CPUs, rebuilds a normal SysV dynamic section where each import's name *is* the NID string, and writes a standalone Linux PIE ELF, a Windows PE, or a macOS Mach-O. Bundled game modules (`sce_module/*.prx`) are converted the same way. There is no launcher process; the output is run directly and the OS loader (`ld.so`, or a generated entry stub on Windows) binds the imports to AnyPS5's libraries.
2. **Native system libraries** (`core/libs/prx`, 352 directories, ~204k LOC excluding tests): one shared library per Sony PRX, exports renamed to NIDs post-link by `nid_patcher`. `libkernel` maps threads, memory, files, time, signals to pthreads/Win32; `libc` is a full replacement C runtime with a guest heap in a reserved arena; `libSceAgcDriver` (79k LOC) decodes PM4 command buffers and drives Vulkan. Unimplemented functions throw `std::runtime_error` and the process terminates. Nothing is silently stubbed.
3. **A shader recompiler** (`core/shader/recompiler`, 71k LOC): RDNA2 decode, control-flow structuring, IR, optimization, SPIR-V emission, multi-level caches, pipeline journaling.

Plus the glue that makes it a community project: a TOML game-patch format, `import_audit.py` (classify every import as implemented/stub/absent before launch), `progress.py` (coverage badges), per-library "debt" documents, a WASM build of the relinker for a browser-based converter, CI on lavapipe, and strict contribution rules (no SDK headers, semantics only from public ISA docs/LLVM/Mesa/FreeBSD or hardware measurement, never replace shipped middleware, never special-case a title).

### 1.1 The eight facts that make "no emulation" possible on PS5

| # | PS5 enabler | Evidence in AnyPS5 |
|---|---|---|
| E1 | **Same ISA.** Zen 2 x86-64 code runs unchanged on any AVX2 PC. | `core/relinker/codegen` only lowers a handful of AMD-only instructions. |
| E2 | **FreeBSD-derived userland ABI.** SysV calls, ELF, libthr threads, FS-based TLS. | `LinuxElfPatcher` sets a glibc interpreter; MinGW builds use `sysv_abi`. |
| E3 | **All OS access through imported, name-identified functions.** | `SysVDynamicSectionBuilder` emits NID strings as symbol names. |
| E4 | **Games make no raw syscalls.** | The relinker refuses any `syscall` instruction ("Forbidden syscall instruction"). |
| E5 | **No JIT in retail code.** | Dynamic loading maps to `dlopen`/`LoadLibrary`. |
| E6 | **Public GPU ISA and command format** (AMD RDNA2, PM4). | `tools/rdna_isa.txt` from AMD's machine-readable ISA; semantics from LLVM/Mesa; `hw-oracle` on retail AMD GPUs. |
| E7 | **Unified memory.** Guest memory is host memory; Vulkan imports it in place. | `VK_EXT_external_memory_host`, dma-buf, write-watch. |
| E8 | **Flat 47-bit address space with known limits.** | Guest arena below the PS5 map limit; fixed-base options. |

---

## 2. Mapping each enabler onto the Switch

| # | Switch 1 reality | Status | Consequence |
|---|---|---|---|
| E1 | Cortex-A57, **AArch64 (ARMv8.0-A)**. Does not run on x86-64. Runs on every ARM64 host CPU. | **Different** | Two execution tiers. Tier 1 (ARM64 hosts): native, in-process, after patching ~5 classes of privileged instructions (proven by yuzu NCE). Tier 2 (x86-64 hosts): static recompilation AArch64 → C/LLVM → native (proven per-game by LAS Recomp, Sky Recomp, HWDER, SMB Wonder, ssbu-recomp; generically but inside an emulator by Suyu 0.0.4). |
| E2 | Horizon OS. AAPCS64 calling convention, Itanium C++ ABI, **musl libc + libc++ live inside `sdk`**, ELF-style dynamic linking performed by `rtld` by symbol name, TLS through a 0x200-byte Thread Local Region reached via `tpidrro_el0`, ELF TLS pointer in `tpidr_el0`. | **Mostly compatible** | Host loader can bind imports after NSO → ELF conversion (nx2elf proves the conversion). TLS register reads must be redirected (same patch pass as E1). Whether the Horizon ABI reserves `x18` is **not publicly documented** and must be measured; it matters for macOS and Windows on ARM. |
| E3 | `main` imports **by mangled name** from `sdk` (nnSdk, 21.5k exports in SDK 9.3) and from the game's own `subsdkN` libraries. Measured: Hades `main` has 631 imports, all by name (244 libc, 68 libc++, 100 `vk*`, 44 `nn::os`, 30 `nn::hid`, 29 `nn::fs`, 22 `nn::audio`, 19 `nn::socket`, …). `nvnBootstrapLoader` is an ordinary `sdk` import. | **Better than PS5** | No hash database to maintain. The replacement unit is one library (`sdk`) split into modules, plus optional replacement of `subsdk` graphics drivers (NintendoSDK_Vulkan/OpenGL) by host passthrough. |
| E4 | Supervisor calls (`svc`) and IPC clients live in `sdk` and `rtld`. **No public census of raw `svc`/`mrs` in retail `main` exists.** yuzu's NCE patcher scans every module for `svc`, `mrs tpidrro_el0/tpidr_el0`, `msr tpidr_el0`, `mrs cntpct_el0` and has never observed `mrs cntfrq_el0`. | **Unverified, cheap to verify** | Phase 0 must run a privileged-instruction census over a corpus of `main` NSOs. Whatever is found is handled by the patch pass; `svcSendSyncRequest` in `main` is treated as "forbidden" exactly like AnyPS5's `syscall` rule. |
| E5 | Horizon **forbids RWX**; `SetProcessMemoryPermission` rejects W+X; the only sanctioned runtime codegen is the `jit:u` sysmodule used by the N64 NSO app. Retail games are AOT (Unity ships IL2CPP). | **Stronger than PS5** | No self-modifying code to worry about. This is what makes *static* recompilation (Tier 2) viable at all. Dynamic `nn::ro` NRO loading maps to relinking NROs on demand + `dlopen`. |
| E6 | GPU is Maxwell (GM20B). Shaders ship as **precompiled Maxwell SASS** (documented by envytools, disassemblable with `nvdisasm -b SM53`). NVN itself is **undocumented** but reverse-engineered: deko3d (homebrew NVN-alike), three 2026 NVN-on-Vulkan layers (LAS Recomp family, SwitchGamesRecomp). Two open SASS translators: yuzu `shader_recompiler` (C++, GPL-3 lineage, kept alive by Eden) and `Ryujinx.Graphics.Shader` (C#, MIT, kept alive by Ryubing/Kenji-NX). | **Comparable** | Intercept at the NVN API (like AnyPS5 intercepts GNM/AGC), not at the pushbuffer/Maxwell-engine level like emulators. That removes the GPU command processor and macro emulation entirely; what remains is NVN object semantics, block-linear tiling (games load pre-tiled BNTX textures directly into memory pools), and the shader recompiler. |
| E7 | Same. NVN memory pools are plain CPU memory; SwitchGamesRecomp imports them with `VK_EXT_external_memory_host`. | **Same** | Reuse AnyPS5's guest-memory-to-GPU design. |
| E8 | 39-bit address space for 2.0.0+ titles (36-bit "64BitOld", 32-bit for early titles); **4 KiB page granularity assumed everywhere**. | **Mostly same; page size is a risk** | Fine on 4 KiB Linux ARM64 and all x86-64. Apple Silicon macOS (16 KiB), Asahi Linux (16 KiB), and Android 16 KiB devices need a page-granularity shim in the memory layer. SMB Wonder rounds to 16 KiB on Apple Silicon and works; Eden has an open rework for the same issue. 32/36-bit titles need a reserved low arena. |

**Net:** six of eight enablers hold or are better; the two that differ (ISA, page size) are engineering problems with public, working precedents, not research problems. The one genuinely unknown fact (instruction hygiene of `main`) is measurable in a day.

---

## 3. Execution tiers and host matrix

| Host | Tier | Game code runs as | Known issues | Precedent |
|---|---|---|---|---|
| Linux ARM64, 4 KiB pages (Raspberry Pi 5, Ampere, most SBCs, Android via Termux/chroot) | 1 | Original bytes, patched | None structural | yuzu NCE (in-process native execution of retail Switch code) |
| Android ARM64 (4 KiB and 16 KiB) | 1 | Original bytes, patched | 16 KiB page rounding; Google requires 16 KiB support for Android 15+ targets | yuzu NCE on Android; SMB Wonder Android port |
| Apple Silicon macOS | 1 | Original bytes, patched | 16 KiB pages; `tpidrro_el0` owned by the OS (must patch reads, which we do anyway); `x18` reserved by Apple (macOS 26.4 added `os_set_custom_x18_abi_enabled`); Vulkan via MoltenVK | Ryujinx Hypervisor mode; LAS Recomp (reference platform), SMB Wonder (reference platform) |
| Asahi Linux (Apple Silicon) | 1 | Original bytes, patched | 16 KiB pages only | Box64's 16 KiB shims; Eden PR #4181 |
| Windows on ARM | 1 | Original bytes, patched (PE/ARM64 output) | `x18` holds the TEB pointer; if games use `x18` as a GPR this host needs Tier 2-style re-register-allocation | Wine's `-ffixed-x18` experience |
| Linux x86-64 | 2 | Statically recompiled | Recompiler maturity (jump tables, function discovery); generated-code size (1.4 GB C++ for SMB Wonder; LAS Recomp compiles on the player's machine) | LAS Recomp, Sky Recomp, HWDER, SMB Wonder, ssbu-recomp, Suyu AOT |
| Windows x86-64 | 2 | Statically recompiled (PE output) | Same as above | Same |

Memory ordering is the *easy* direction: ARM64's weak model recompiled onto x86-64's TSO needs no barriers. Exclusive-monitor semantics differ between Cortex-A57 and newer cores; yuzu rewrites exclusives as ordered and we should too.

---

## 4. Evidence base (what already exists, October 2026)

| Project | What it proves for us | License / status |
|---|---|---|
| **yuzu NCE** (Native Code Execution; source survives in Eden, Citron mirrors) | The complete list of privileged-instruction classes in retail Switch code and a working load-time patch strategy: each `svc`/`mrs`/`msr` becomes a `B` into a per-module patch section (±128 MiB), handlers save/restore context; host `tpidr_el0` juggling; `cntpct` emulated via `cntvct`; exclusives rewritten as ordered; 39-bit AS required; 16 KiB hosts unsupported. | GPL (2.0-or-later headers, 3.0 project). Reference only, do not copy. |
| **Ryujinx Hypervisor mode** (macOS) | Alternative Tier 1 design: run guest at EL0 under a tiny EL1 shim via Hypervisor.framework; traps only `svc`, `cntfrq`/`cntpct` reads, data aborts. Shows MoltenVK causes very frequent SVC exits. | MIT (mirrors). Useful fallback design for macOS. |
| **SwitchGamesRecomp / SuperMarioWonderRecomp** (BelmanteGu) | A *shared* runtime across games: Python AArch64→C++ recompiler, Horizon kernel subset (ArbitrateLock/Unlock, condvars, WaitForAddress, events, QueryMemory, 19.2 MHz tick), HIPC parser, `nn::*` HLE registered **by name** ("HLE registry, then exports of loaded modules, then a logging stub"), NVN-on-Vulkan via recorded command lists replayed at `nvnQueueSubmitCommands`, memory pools via `VK_EXT_external_memory_host`, compute-shader untiler, Ryujinx translator run offline. Hades: Vulkan passthrough instead of NVN. 17.8M instructions → 250k functions → 1.4 GB C++. 330 stars, active 2026-10-18. | MIT. **Closest existing thing to a generic AnySwitch.** Takes prod.keys at import (a design we must not copy). |
| **LAS Recomp** (peiikai) and derivatives **Sky Recomp**, **letsgo-recomp**, **AstralChain-PC** | Capstone + Unicorn micro-execution for jump tables; game code compiled on the player's machine with `zig cc` so releases contain no game code; Metal + MetalFX on macOS, Vulkan elsewhere; own SASS translator built from envytools docs; explicit refusal to document dumping. Built almost entirely with Claude Code. 1.4k stars; beta_4 on 2026-10-15. Its NVN layer is reused by two other ports. | MIT. |
| **HWDER** (sonsegajp) | Solo project with a full Maxwell command processor and SASS→SPIR-V at native 60 Hz. Takes an encrypted `.nsp` plus `prod.keys` and decrypts at startup. | The **anti-pattern** legally. |
| **ssbu-recomp / OpenSmash, B2recomp** | ~1,100 `nn*` entry points suffice to run Smash Ultimate; SASS→GLSL→SPIR-V. | GPL-3. Reference only. |
| **Suyu 0.0.4** (final, Sept 2026) | A *generic* AOT AArch64→x86-64 stage for any title, but still inside the emulator's HLE and GPU emulation. | GPL-3; archived. |
| **deko3d** (devkitPro) | The NVN object model and client-side command recording are fully understood ("a year-and-a-half reverse engineering effort centered on NVN"). | Zlib. |
| **nx2elf**, **Ghidra-Switch-Loader**, **oss-rtld** | NSO/NRO → loadable ELF reconstruction is a solved problem; rtld semantics are documented and reimplemented. | nx2elf has **no license** (do not copy); Ghidra loader ISC; oss-rtld ISC (archived 2026-01). |
| **exlaunch**, **LibHakkun**, **SaltyNX**, **imgui-xeno** | On-device frameworks replace `sdk` exports (including `nvnBootstrapLoader`) by name at runtime; retail games tolerate it. | Various. |
| **open-ead/nnheaders**, **nnsdk-rs** | Public reconstructions of `nn::*` signatures and types from unstripped retail binaries (Splatoon 2 ≤3.1.0, SMO, SwSh). | Use for *names and prototypes*; re-derive struct layouts from switchbrew and your own binaries to keep the clean-room argument. |
| **switchbrew.org** | NSO/NRO/MOD0, rtld, SVC list and ABI, Thread Local Region layout, memory layout, HID/FS/Audio/NV/VI service semantics, GPU shader header. | Primary public source for everything kernel/IPC. |

The important shift relative to AnyPS5's situation: **the Switch community reached "native execution" from the recompilation side first, per game.** Nobody has yet shipped the generic, drop-a-dump-in converter that AnyPS5 is for PS5. That is the gap this project would fill, and the per-game projects are the best source of ground truth for what a game actually needs.

---

## 5. Component-by-component feasibility

Sizes are rough, from AnyPS5's measured sizes and the recomp projects' published numbers. "LOC" excludes tests.

### 5.1 Relinker (NSO/NRO → native ELF / Mach-O / PE)
- Parse NSO0 (LZ4/zstd segments, MOD0 → `.dynamic`, `.dynsym/.dynstr`, `.eh_frame_hdr`, bss, build-id), NRO0, NPDM (address-space width, heap size, thread priorities, capabilities).
- Emit a normal AArch64 ELF PIE (or Mach-O/PE) whose DT_NEEDED list points at our libraries and whose imports keep their mangled names. Drop `rtld` and `sdk`; convert `subsdkN` like AnyPS5 converts bundled PRX modules; optionally drop `subsdk` graphics drivers (NintendoSDK_Vulkan/OpenGL) in favour of host passthrough.
- Relocations: ABS64, GLOB_DAT, JUMP_SLOT, RELATIVE only (per switchbrew rtld). No NID stripping needed.
- **Easier than AnyPS5's relinker.** Estimate 8–15k LOC. nx2elf is the proof but is unlicensed; write from switchbrew.

### 5.2 Privileged-instruction patcher (Tier 1)
- Scan `.text` for: `svc #imm`, `mrs tpidrro_el0`, `mrs/msr tpidr_el0`, `mrs cntpct_el0/cntvct_el0/cntfrq_el0`, `mrs dczid_el0`/`dc zva`, `ic`/`isb` sequences, and exclusives (`ldxr/stxr` family, rewrite as ordered). Replace each with `B` into a patch section placed within ±128 MiB; handler does the work and branches back.
- `tpidrro_el0` cannot be set from userland on Linux or macOS, so the TLR pointer lives in a host-TLS slot with a link-time-known offset; the handler is ~3 instructions. Keep host `tpidr_el0` untouched at all times (unlike yuzu NCE), because our native libraries are called directly by the game and rely on host libc TLS.
- SVC safety net: implement only the handful of SVCs that could plausibly appear in `main` (SleepThread, GetInfo, OutputDebugString, arbitration/address waits, QueryMemory). `svcSendSyncRequest` and friends in `main` are a hard error, mirroring AnyPS5's forbidden-syscall rule.
- Estimate 3–6k LOC. yuzu NCE's patcher is ~1k lines of the same logic.

### 5.3 Static recompiler (Tier 2, x86-64 hosts)
- AArch64 → C or LLVM IR, per function, N64Recomp/XenonRecomp lineage. Function discovery is unusually good on Switch: `.eh_frame_hdr` is mandatory (MOD0 points at it) and `.dynsym` names all exported functions. Jump tables via pattern matching plus Unicorn micro-execution (LAS Recomp). Indirect calls via address → function hash table. No self-modifying code (E5).
- Known cost problem: 1.4 GB of generated C++ for a large title, 6-minute recompile, 15-minute first build. Options: emit LLVM IR/object code directly (no C round-trip), per-function deduplication, compile on the player's machine (LAS Recomp model, which also keeps game code out of releases).
- Shares everything above the ISA line with Tier 1 (same native `sdk`, NVN, shaders).
- Estimate 25–40k LOC plus a large differential test suite (run the same function natively on ARM64 vs recompiled on x86-64). This is the single largest technical risk and should come *after* Tier 1 works.

### 5.4 Native nnSdk (`sdk` replacement)
- One NSO on the console, but 21.5k exports. Split into host libraries by namespace exactly as AnyPS5 splits by PRX: `nn_os`, `nn_fs`, `nn_hid`, `nn_audio`, `nn_vi`, `nn_oe`/applet, `nn_account`, `nn_ro`, `nn_socket`/`nifm`/`ssl` (mostly offline stubs), `nn_time`, `nn_settings`, `nn_diag`, `nn_prepo` (telemetry → no-op), `nn_swkbd`/`web`/`err` (dialogs), `nn_init`, `nn_mem`, plus `libc_nx` (musl surface, with a guest heap in a reserved arena) and `libcxx_nx` (libc++ ABI surface).
- Long-tail shape is the same as PS5: ~600 imports per typical title, ~1,100 entry points for a big title (ssbu-recomp), vs 21.5k total. First playable subset: 30–60k LOC. Mature: 150k+ LOC (AnyPS5 is at 204k after ~2.5 months of community work).
- Horizon kernel semantics needed are small and documented: locks/condvars via ArbitrateLock/WaitProcessWideKeyAtomic, events, address arbitration, 19.2 MHz tick, TLR layout, thread priorities/cores (map to host threads; ignore affinity).

### 5.5 NVN on Vulkan
- Hundreds of `nvn*` functions bound by name through `nvnDeviceGetProcAddress`. Objects: Device, Queue, CommandBuffer, MemoryPool, Buffer, Texture/TextureView, Sampler, Program/Shader, Sync, Window (presentation via our swapchain instead of vi/nvnflinger). Record NVN command-buffer calls into our own command list; replay into Vulkan at `nvnQueueSubmitCommands`.
- Block-linear tiling must be handled (pre-tiled BNTX textures are written straight into pools); compute-shader untiler is the proven approach.
- Risks: games reading NVN object fields directly via inlined `nn::gfx` helpers; memory-pool coherence; readbacks. No headers exist, so the API is reverse-engineered from binaries and deko3d's public design.
- Estimate 30–60k LOC (AnyPS5's AGC driver is 86k but includes a PM4 command processor we do not need).
- **Cheap early win:** titles that ship a `NintendoSDK_Vulkan` or `_OpenGL` `subsdk` (Hades: 100 `vk*` imports) can be run by *passthrough* to the host Vulkan driver with SPIR-V shaders untouched, no NVN, no SASS translation. SwitchGamesRecomp did exactly this for Hades.

### 5.6 Maxwell SASS → SPIR-V shader recompiler
- Mandatory (E6). Inputs are SM5x SASS blobs with the 0x50-byte Nvidia program header, scheduling control words every fourth slot.
- Options: (a) port `Ryujinx.Graphics.Shader` (MIT, C#) to C++ or embed .NET; (b) write a new one from envytools/nvdisasm documentation, as LAS Recomp did; (c) use yuzu's (GPL-3; forces the whole project to GPL-3 and depends on DMCA-pressured forks). Recommend (a) as an offline/cached stage first, (b) as the long-term native path.
- Estimate 25–45k LOC for an own implementation (AnyPS5's RDNA recompiler is 71k).
- Switch 2 would need an SM86 (Ampere) frontend; none exists publicly. Design the frontend as a pluggable ISA module from day one.

### 5.7 Everything else
- **Audio:** `audren` (command-list renderer: voices, mixes, effects, revision-gated) and `audout` (buffer queue) onto SDL3/miniaudio. Medium.
- **HID:** `nn::hid` Npad styles (Pro/handheld/Joy-Con), touch, six-axis, vibration onto SDL3 gamepads + keyboard/mouse mapping file. Small, but the shared-memory LIFO ring model must be honoured if any game reads it directly (unlikely in `main`).
- **FS:** RomFS mount, save-data mount with journaled commit, cache/temp storage, add-on content. Small-medium.
- **nn::ro:** runtime NRO loading → run the relinker in-process (or at install time) and `dlopen`. Medium.
- **Applet/oe:** focus, operation mode, performance mode, exit requests. Small.
- **Account / friends / nifm / socket / ssl / bcat / nex:** single local user, offline, loud failures. Small, long tail.
- **Fonts:** games draw system text through `nn::pl` shared fonts; user-supplied dump or open substitutes (AnyPS5 uses Noto). Small.
- **Tooling:** import audit, symbol inventory per title, progress badges, TOML patches, debt docs, WASM web converter, lavapipe CI, differential recompiler tests. ~15–20k lines of Python/JS, as in AnyPS5.

---

## 6. Risks, ranked

| # | Risk | Severity | Mitigation |
|---|---|---|---|
| R1 | **Nintendo legal action** (§1201 trafficking theory; 2024–2026 track record; "primarily designed to play Nintendo games" applies to a relinker). | High | Never read, store, or require keys; refuse encrypted input; ship no game data, fonts, firmware, or SDK-derived headers; no dumping instructions anywhere; releases contain no game code (compile recompiled output on the user's machine); maintain a non-GitHub mirror; name the project without Nintendo marks; clean-room sourcing rules enforced in CONTRIBUTING like AnyPS5. Residual risk remains: Nintendo could choose a recomp to set precedent. |
| R2 | **Generic x86-64 recompilation does not reach "any game" quality** (jump tables, function boundaries, FP corner cases, code size). | High | Ship Tier 1 first; gate Tier 2 on a differential test corpus; adopt per-title TOML overrides like XenonRecomp; emit LLVM IR not C. |
| R3 | **16 KiB-page hosts** (macOS, Asahi, Android 16K). | Medium | Page-granularity shim in the memory layer; reserve guest arena with 16 KiB-rounded mappings; follow Eden #4181 and SMB Wonder. |
| R4 | **NVN semantics without headers**; games that peek at NVN object internals. | Medium | Treat deko3d and the recomp NVN layers as the spec; loud assertion on every unknown field access; per-title patches. |
| R5 | **Shader translator supply**: both open translators are maintained by forks under takedown pressure. | Medium | Vendor an MIT snapshot as a submodule mirror; plan an own implementation. |
| R6 | **`main` instruction hygiene unknown** (raw `svc`, `mrs tpidr*`, timers). | Low (cheap to measure) | Phase 0 census; whatever appears is handled by the patcher; refuse IPC SVCs. |
| R7 | **`x18` / `tpidrro_el0` on macOS and Windows on ARM.** | Medium for those hosts | Measure `x18` usage in a corpus; if used, those hosts need register renaming (a Tier 2 transformation) or the Hypervisor.framework route. |
| R8 | **Switch 2 unavailable**; NVN2/SM86 unknown. | Blocking for Switch 2 | Keep ISA/GPU interfaces pluggable; do not promise Switch 2. |
| R9 | **Scale and expectations.** AnyPS5's pace is a 44k-star community with thousands of PRs. | Medium | Milestone-driven plan; publish the bring-up ladder and progress badges early to attract contributors. |
| R10 | **Trademark** in the name ("Switch"). AnyPS5 uses "PS5" without incident, but Nintendo is more litigious. | Low-Medium | Prefer a non-descriptive name; keep "Switch" out of the binary/repo name. |

---

## 7. Comparison summary: AnyPS5 vs AnySwitch

| Dimension | AnyPS5 (PS5 → PC) | AnySwitch (Switch 1 → PC/Mac/ARM) |
|---|---|---|
| CPU | Native (same ISA) | Native on ARM64 hosts after patching; static recompile on x86-64 |
| Import binding | By NID (SHA-1 hashes) | By symbol name (easier) |
| System library unit | 352 PRX libraries | 1 NSO (`sdk`) split into ~20 host libraries |
| Graphics interception | GNM/AGC command buffers (PM4 decode needed) | NVN API calls (no command processor needed) |
| Shader ISA | RDNA2 (public ISA docs) | Maxwell SASS (envytools docs; two open translators) |
| Raw syscalls in games | None (enforced) | Expected none in `main`; must verify |
| JIT in games | Rare, supported via memfd | Forbidden by the OS (helps static recompilation) |
| Page size | 4 KiB / 16 KiB-agnostic on x86 | 4 KiB assumed; 16 KiB hosts need shims |
| Input the user provides | Decrypted ELF + app0 | Decrypted `exefs` + `romfs` (must never touch keys) |
| Platform owner's posture | No action against shadPS4/AnyPS5 | Settlements, judgments, ~420 repos removed in 2026 |
| Prior art | shadPS4 (emulator) | yuzu NCE, Ryujinx HV, five+ per-game recomps, one shared recomp framework |

**Conclusion:** the project is feasible for Switch 1 and has more direct prior art than AnyPS5 had, but it is a two-ISA project with a harder legal environment. Switch 2 is out of reach until decrypted binaries exist. The plan in [PLAN.md](PLAN.md) sequences the work so that the cheapest decisive experiments come first and the x86-64 recompiler, the most expensive component, is built on top of a runtime already proven on ARM64.

---

## Sources

AnyPS5: https://github.com/boykopovar/AnyPS5 (docs/dev/ARCHITECTURE.md, core/relinker, core/libs/prx, core/shader/recompiler; measured 2026-10-20 at commit e9cc4a8). Press: https://www.techpowerup.com/353098/anyps5-project-skips-emulation-entirely-aims-to-port-playstation-5-games-to-pc-directly , https://www.theweek.in/news/sci-tech/2026/10/13/anyps5-challenges-sony-digital-future.html

Switch formats and ABI: https://switchbrew.org/wiki/NSO , https://switchbrew.org/wiki/NRO , https://switchbrew.org/wiki/MOD , https://switchbrew.org/wiki/Rtld , https://switchbrew.org/wiki/SVC , https://switchbrew.org/wiki/Thread_Local_Region , https://switchbrew.org/wiki/Memory_layout , https://switchbrew.org/wiki/JIT_services , https://switchbrew.org/wiki/GPU_Shaders , https://switchbrew.org/wiki/NV_services , https://switchbrew.org/wiki/Display_services , https://switchbrew.org/wiki/Audio_services , https://switchbrew.org/wiki/HID_services , https://switchbrew.org/wiki/Filesystem_services , https://switchbrew.org/wiki/RO_services , https://raw.githubusercontent.com/Atmosphere-NX/Atmosphere/master/libraries/libmesosphere/source/arch/arm64/svc/kern_svc_handlers_asm.s

Prior art: yuzu NCE patcher https://git.dawidpotocki.com/mirror/yuzu-mainline/plain/src/core/arm/nce/patcher.cpp ; Ryujinx Hypervisor https://git.axenov.dev/Museum/ryujinx/commit/a53cfdab78c382677eb826bd5bedb58b3b838796 ; SwitchGamesRecomp https://github.com/BelmanteGu/SuperMarioWonderRecomp (docs/FRAMEWORK.md, docs/NVN.md, docs/FINDINGS.md, docs/RUNTIME.md) ; LAS Recomp https://github.com/peiikai/LASRecomp ; Sky Recomp https://github.com/Apolinario-coder/SkyRecomp ; HWDER https://github.com/sonsegajp/HWDER ; ssbu-recomp https://github.com/Denkishi/ssbu-recomp ; Suyu recompiler mode https://www.digitalcitizen.life/suyu-0-0-4-adds-static-recompilation-to-improve-nintendo-switch-emulation-on-pc/ ; deko3d https://github.com/devkitPro/deko3d ; nx2elf https://github.com/shuffle2/nx2elf ; Ghidra-Switch-Loader https://github.com/Adubbz/Ghidra-Switch-Loader ; oss-rtld https://github.com/Thog/oss-rtld ; exlaunch https://github.com/shadowninja108/exlaunch ; LibHakkun https://codeberg.org/fruityloops1/LibHakkun ; nnheaders https://github.com/open-ead/nnheaders ; nnsdk-rs https://github.com/ultimate-research/nnsdk-rs ; Eden https://git.eden-emu.dev/eden-emu/eden (NCE 16K rework issue #3607 / PR #4181) ; Ryujinx shader translation https://www.mintlify.com/yakushabb/mirror-ryujinx/architecture/graphics/shader-translation ; tegra block-linear https://docs.rs/tegra_swizzle

Switch 2: https://switchbrew.org/wiki/Switch_2:_Tegra_T239 , https://www.nintendolife.com/news/2025/05/nintendo-switch-2-final-tech-specs-have-been-confirmed , https://www.nintendo.com/us/whatsnew/ask-the-developer-vol-16-nintendo-switch-2-part-4/ , https://github.com/pound-emu/pound , https://heldgames.com/guides/switch-2-emulators , https://www.notebookcheck.net/Switch-2-Developer-unveils-a-universal-exploit-that-works-entirely-offline.1347373.0.html , https://en-americas-support.nintendo.com/app/answers/detail/a_id/68586

Recompilation state of the art: https://github.com/N64Recomp/N64Recomp , https://github.com/hedge-dev/XenonRecomp , https://github.com/lifting-bits/remill , https://arxiv.org/abs/2609.16423 , https://arxiv.org/abs/2605.08419 , https://box86.org/2026/08/preview-of-next-box64-improved-non-4k-pagesize-handling/ , https://developer.apple.com/forums/thread/841302 , https://lists.llvm.org/pipermail/llvm-dev/2019-May/132398.html

Legal: https://www.gamingonlinux.com/2024/02/nintendo-goes-after-switch-emulator-yuzu-in-new-lawsuit , https://www.engadget.com/makers-of-switch-emulator-yuzu-quickly-settle-with-nintendo-for-24-million-203204698.html , https://www.gamingonlinux.com/2024/10/nintendo-switch-emulator-ryujinx-gets-shut-down , https://www.escapistmagazine.com/news-nintendo-switch-emulator-dmca-github/ , https://www.nintendolife.com/news/2026/08/nintendo-wipes-out-hundreds-of-switch-emulator-repositories , https://www.dexerto.com/gaming/nintendo-wins-2-million-in-lawsuit-against-switch-hack-maker-3246885/ , https://stevivor.com/news/nintendo-of-america-secures-4-5-million-default-judgment-against-reddit-piracy/ , https://hothardware.com/news/nintendo-lawyer-emulation-stance , https://www.xda-developers.com/switch-zelda-link-awakening-remake-just-got-native-pc-port-nintendo-probably-isnt-thrilled/ , https://github.com/Zelda64Recomp/Zelda64Recomp
