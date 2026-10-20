# AnySwitch — Project plan

*Companion to [FEASIBILITY.md](FEASIBILITY.md). Written 2026-10-20.*
*"AnySwitch" is a working title only; see decision D9.*

---

## 0. Progress

**2026-10-20: repository created; Phase 0 tooling and the core of Phase 1 implemented.**

- Phase 0: `anyswitch-relink census`, `audit`, `symbols`, `provided`; `tools/phase0/priv_census.py` and `symbol_inventory.py` are ready to run on owned titles. The measurements themselves (Q1-Q8) are still open.
- Phase 1: relinker (NSO/NRO → AArch64 Linux PIE/shared object), all Tier 1 patch classes, traps for missing imports, runtime start-up, guest threads/TLS, SVC safety net, crash reports. Synthetic corpus of six end-to-end cases passes under qemu-user; host tools build with GCC, Clang and MSVC.
- Not yet: Phase 1 exit criterion "first retail `main` reaches `nn::init::Start`" needs a measured title; a homebrew NRO built with libnx has not been tried.
- Decision D6 changed to MIT (see REUSE.md). AGENTS.md now holds the contribution rules for people and AI agents.

## 1. Goal and non-goals

**Goal.** A tool that takes a user's own decrypted Switch 1 title (`exefs` + `romfs`) and produces a native application for Linux, Windows, and macOS, with no emulator at runtime, in the same way AnyPS5 does for PS5 titles: a relinker, a native reimplementation of the system library the game links against, an NVN-on-Vulkan graphics layer, and a Maxwell shader recompiler.

**Non-goals (for the foreseeable future).**
- Switch 2 titles. Nothing can be built or tested until decrypted Switch 2 binaries exist publicly. Section 9 lists the preparatory design choices that keep the door open.
- AArch32 titles (a small number of early games). Out of scope for Tier 1 on Apple Silicon (no AArch32 EL0); possible later through Tier 2 only.
- Online services (Nintendo Switch Online, nex, bcat, friends). Offline single-user only, failing loudly.
- Anything involving keys, decryption, dumping, firmware, or system fonts. The tool refuses encrypted input and ships none of these.
- Running Switch *system* modules (applets, sysmodules). Applications only.

---

## 2. Decisions (ADR-style)

**D1. Intercept at the `sdk` boundary, not at SVC/IPC.**
Retail `main` modules import libc, libc++, every `nn::*` family, and NVN by name from the `sdk` NSO. We replace `sdk` with native libraries that export the same mangled names. We do *not* run Nintendo's nnSdk code and do *not* emulate Horizon IPC services (the emulator approach). Rationale: this is the AnyPS5 model (PRX = `sdk`), it is the thinnest layer that is fully name-addressable, and it removes the NVN driver, nvdrv ioctls, and the Maxwell command processor from scope. A tiny SVC safety net exists for stray `svc` instructions in `main`; IPC SVCs in `main` are a hard error.

**D2. Two execution tiers sharing one runtime.**
Tier 1 (ARM64 hosts): original game bytes run in-process after load-time patching of privileged instructions (the yuzu NCE technique, reimplemented clean). Tier 2 (x86-64 hosts): a static recompiler lowers AArch64 to LLVM IR/C and the result is linked against the *same* native runtime. Tier 1 ships first because it validates the runtime without recompiler bugs in the loop.

**D3. Graphics at the NVN API level on Vulkan; shaders recompiled offline and cached.**
NVN calls are recorded into our own command list and replayed into Vulkan on submit. MoltenVK on macOS initially (a Metal backend is a later optimisation, as LAS Recomp shows). Maxwell SASS → SPIR-V happens at first use and is cached on disk with versioned entries, as in AnyPS5. Titles that ship their own `NintendoSDK_Vulkan`/`_OpenGL` `subsdk` get host passthrough instead of NVN.

**D4. Input contract: decrypted `exefs` + `romfs` only.**
No `prod.keys`, no `.nsp`/`.xci`, no title keys, ever, in any code path or document. The relinker detects encrypted/compressed-container input and stops with a message that does not explain how to decrypt. Releases contain no game-derived data; Tier 2 recompiled code is generated and compiled on the user's machine.

**D5. Clean-room sourcing rules (copied from AnyPS5's CONTRIBUTING, adapted).**
Semantics come from switchbrew, envytools/nvdisasm output, deko3d, public papers, Mesa/nouveau, musl, libc++, and measurements on the user's own hardware/binaries. No leaked SDK headers, no NVN headers, no copied emulator code. `nn::*` *names and prototypes* may be taken from public symbol reconstructions (open-ead/nnheaders, nnsdk-rs); struct layouts are re-derived and documented with their source. Every reimplemented function cites its source in the debt/notes file for its module.

**D6. License: MIT for first-party code; third-party only as MIT/BSD/0BSD/zlib/Apache-2.0 submodules; GPL projects used only as out-of-process tools or as read-only references.**
Revised after the reuse survey in [REUSE.md](REUSE.md): every directly reusable Switch-side implementation (Ryujinx audio renderer, shader translator, texture code and ARMeilleure; dynarmic 0BSD; SwitchGamesRecomp and the LAS Recomp family; N64Recomp/XenonRecomp; envytools; Mesa; deko3d; tegra_swizzle) is MIT or more permissive, and MIT lets the per-game recomp projects consume our runtime in return. Apache-2.0 dependencies (SPIRV-Tools, SPIRV-Cross, MoltenVK, Remill, Ghidra) are compatible with MIT but not with GPL-2.0-only. The cost is that AnyPS5's GPL-2.0-only code can only be a design reference. Never incorporate GPL code (yuzu/Eden lineage, ssbu-recomp, exlaunch, Atmosphère, Unicorn in-process) into the tree; Unicorn is allowed as a separate convert-time executable.

**D7. Fail loud.**
Unimplemented or unexpected states throw and terminate with the function name, exactly like AnyPS5's `NotImplemented` convention. Silent stubs only for UI-only functions and only when listed in the module's debt file. `import_audit` must classify every import of a title before launch.

**D8. Hosting and continuity.**
Primary repository on GitHub for visibility; automatic mirror to a self-hosted Forgejo or Codeberg from day one; release artefacts also mirrored. CI never contains game data; the test corpus is synthetic (see 5.1) plus homebrew.

**D9. Naming.**
Avoid Nintendo marks in the final name, repository, and binary names. "AnySwitch" is used in these documents only.

**D10. Language and toolchain.**
C++20 for runtime, relinker, patcher, and shader recompiler (same as AnyPS5, eases cross-pollination). Python for tooling. Clang/LLVM for the Tier 2 backend. CMake + Ninja. SDL3 for window/input/audio. Vulkan 1.3 (+ MoltenVK).

---

## 3. Architecture

```
           user's decrypted title
           exefs/ {main, main.npdm, rtld, sdk, subsdk0..9}   romfs/
                    │
                    ▼
   ┌──────────────────────────────────────────────┐
   │ core/relinker                                │
   │  NSO/NRO/NPDM parser → module graph          │
   │  drop rtld + sdk; keep main + subsdkN        │
   │  [Tier1] patcher: svc/mrs/msr/exclusives     │
   │  [Tier2] recomp: AArch64 → LLVM IR → object  │
   │  emit ELF (Linux) / Mach-O (macOS) / PE (Win)│
   │  imports keep mangled names; DT_NEEDED →     │
   │  our libraries; TOML title patches           │
   └──────────────────────────────────────────────┘
                    │  app.elf / app.exe / app.app  (standalone, no launcher)
                    ▼
   ┌──────────────────────────────────────────────┐
   │ core/libs/nx  (the native "sdk")             │
   │  nx_init  nx_os  nx_mem  nx_fs  nx_ro        │
   │  nx_hid   nx_audio  nx_vi  nx_oe  nx_account │
   │  nx_time  nx_settings  nx_diag  nx_prepo     │
   │  nx_socket/nifm/ssl (offline)  nx_swkbd/err  │
   │  libc_nx (musl surface + guest heap arena)   │
   │  libcxx_nx (libc++ ABI surface)              │
   │  nx_svc (safety-net SVC handlers)            │
   │  nvn (NVN → Vulkan)   vkpass (subsdk Vulkan) │
   └──────────────────────────────────────────────┘
                    │
   ┌──────────────────────────────────────────────┐
   │ core/shader/maxwell                          │
   │  SASS(SM5x) decode → CFG → IR → opt → SPIR-V │
   │  pluggable ISA frontend (SM86 slot for later)│
   │  disk cache, prebuilt packs, pipeline journal│
   └──────────────────────────────────────────────┘
```

**Repository layout (mirrors AnyPS5 so contributors can move between the two):**

As built (October 2026); planned folders are marked "later":

```
core/abi/           nxabi.h, the relinker/runtime contract
core/common/        bytes, file IO, SHA-256, errors
core/format/        NSO, NRO, NPDM, MOD0, ELF reader, LZ4, input-contract checks
core/relinker/      a64 encoders, patcher, census, ELF writer, registry, CLI (info/symbols/census/audit/provided/module/exefs)
core/recomp/        later: aarch64 lifter → LLVM IR (Tier 2)
core/shader/maxwell/ later: decoder, controlflow, ir, opt, spirv backend, cache
runtime/src/<module>/ the native sdk, one folder per SDK area (rt, nn_os, libc, cxxabi so far), built into libnxrt.so
tests/unit/         host unit tests
tests/corpus/       synthetic guest programs built from source into NSO/NRO
tools/              dev/ (build, test), bootstrap/ (toolchain), corpus/ (builder, runner), phase0/ (priv_census, symbol_inventory), check_conventions.py
docs/dev/           ARCHITECTURE.md, BUILD.md, TESTING.md, CONVENTIONS.md, RECIPES.md, debt/<module>.md
docs/user/          USAGE.md, INPUT_MAPPING.md, GAME_TESTING.md
demo/web/           WASM build of the relinker (later)
3rdparty/           submodules only, each with PROVENANCE.md entry: SDL3, Vulkan-Headers, volk, VMA, SPIRV-Tools, SPIRV-Cross, lz4, zstd, capstone (prototyping), mimalloc, tomlplusplus, Catch2, Arm AARCHMRS_BSD data, envytools tables, dynarmic (A64 frontend), deko3d (texture layout), tegra_swizzle, ryujinx snapshot (Graphics.Shader, Audio, Graphics.Texture, ARMeilleure, Cpu/AppleHv), SuperMarioWonderRecomp + LASRecomp snapshots (runtime, nvn, recomp). Unicorn, zig cc, nvdisasm, lavapipe, Emscripten are tools, not submodules.
```

The per-component map of what is vendored, ported, used as a tool, or only read is in [REUSE.md](REUSE.md).

---

## 4. Key mechanisms (design notes that de-risk the phases)

**4.1 NSO → ELF.** Decompress segments (LZ4, zstd), follow MOD0 to `.dynamic`, rebuild `.dynsym/.dynstr/.hash/.rela.dyn/.rela.plt/.got/.plt/.init_array/.fini_array/.eh_frame(_hdr)/.bss`, set `PT_INTERP` to the host loader, emit `DT_NEEDED` for our modules, keep `DT_RUNPATH=$ORIGIN/libs`. `subsdkN` become shared objects with the same treatment. Verify by round-tripping homebrew NROs built with devkitPro.

**4.2 Patcher (Tier 1).** Scan `.text` once; for each hit emit a 4-byte `B` to a handler in a patch section placed adjacent to the module (±128 MiB). Handlers:
- `svc #n` → save caller-saved state, call `nx_svc::dispatch(n, ctx)`, restore, branch back. Only SleepThread, GetInfo, OutputDebugString, GetSystemTick, QueryMemory, ArbitrateLock/Unlock, WaitProcessWideKeyAtomic/SignalProcessWideKey, WaitForAddress/SignalToAddress are implemented; anything else terminates with the SVC number.
- `mrs xN, tpidrro_el0` → load the per-thread TLR pointer from a host-TLS slot at a link-time-known offset (3 instructions). Host `tpidr_el0` is never touched.
- `mrs/msr xN, tpidr_el0` → read/write the guest ELF-TLS pointer stored next to the TLR pointer.
- `mrs xN, cntpct_el0 / cntvct_el0` → host monotonic counter rescaled to 19.2 MHz; `cntfrq_el0` → constant 19200000. (Host `cntfrq` differs: Apple 24 MHz, Pi 54 MHz.)
- `dc zva`/`dczid_el0` → host equivalent or memset path; `ic ivau` → no-op.
- `ldxr/stxr` families → `ldaxr/stlxr` (ordered), as yuzu does for post-A57 cores.
Threads created by `nx_os::CreateThread` allocate a 0x200-byte TLR (first 0x100 bytes unused) and the guest ELF-TLS block, set the host-TLS slots, then run the guest entry.

**4.3 Memory model.** One reserved guest arena sized from NPDM (address-space width) with 4 KiB bookkeeping over 16 KiB host pages where needed; `nn::os` memory-block/heap APIs and NVN pools allocate inside it; Vulkan imports it in place via `VK_EXT_external_memory_host` (Linux/Windows) or MoltenVK host-visible buffers (macOS). CPU-write tracking by write-watch (Windows) or `mprotect` + SIGSEGV (POSIX), reused from AnyPS5's design.

**4.4 NVN.** `nvnBootstrapLoader("nvnDeviceGetProcAddress")` returns our resolver; every `nvn*` entry point is a native function. Objects are opaque structs of the sizes games allocate (sizes are public through deko3d and binary analysis). Command buffers are recorded as our own op stream; `nvnQueueSubmitCommands` replays into a Vulkan command buffer with dynamic rendering, extended dynamic state, and graphics pipeline libraries. Textures in pools are untiled by a compute pass on upload; render targets are host images with write-back to pools on explicit NVN barriers/finishes only.

**4.5 Shader recompiler.** Shader binaries are discovered from NVN `ShaderData` structures at `nvnProgramSetShaders` time (plus optional romfs pre-scan for warm caches). Pipeline: SASS decode (control words every fourth slot) → basic blocks/CFG → structurizer → SSA IR → optimisation (constant folding, DCE, texture/sampler binding resolution, attribute/varying layout) → SPIR-V emitter → optional SPIRV-Tools validate/optimise. Cache keyed by shader hash + translator version + GPU/driver fingerprint. Phase 3 may call a vendored MIT translator out of process to get a first frame sooner; the native recompiler replaces it.

**4.6 Tier 2 recompiler.** Function set = union of `.dynsym`, `.eh_frame_hdr` FDE starts, direct `bl` targets, and relocation targets. Per function: decode with an own table-driven AArch64 decoder (Capstone acceptable for prototyping), lift to LLVM IR with explicit guest register state in a context struct, NZCV as separate i1 values, NEON as LLVM vectors, memory through the guest arena base. Jump tables: recognise the two Clang idioms (adr+ldrb/ldrh/ldrw scaled table, and `br` after `ldr` from a table) and confirm by micro-execution (Unicorn) when ambiguous. Indirect calls/branches go through a perfect-hash address→function table; falling off the table terminates with the target address. Exceptions: no C++ unwinding across recompiled code initially (treat `__cxa_throw` as fatal, measure how often games rely on it). Output: object files compiled with Clang on the user's machine, linked with the same runtime as Tier 1.

---

## 5. Phases, milestones, exit criteria

Each phase has a bring-up ladder, like AnyPS5's "Game bring-up" discussions. Rough durations assume 1–2 experienced systems developers full time; a community doubles pace after Phase 3.

### Phase 0 — Decisive experiments (2–4 weeks)
Goal: replace every "unknown" in FEASIBILITY.md section 6 with a measurement before committing to architecture.
1. **Privileged-instruction census** (`tools/priv_census.py`): over every `main`/`subsdk` NSO the team legally owns, count `svc` (by number), `mrs/msr` of `tpidrro_el0`, `tpidr_el0`, `cntpct/cntvct/cntfrq_el0`, `dczid_el0`, `dc`/`ic` ops, exclusives, and `x18` reads/writes. Report per title. This decides how much of 4.2 is needed and whether macOS/Windows-on-ARM are Tier 1 hosts.
2. **Import inventory** (`tools/symbol_inventory.py`): union and frequency of `sdk` imports across the corpus, grouped by namespace. This is the implementation order for `core/libs/nx` and the content of the progress badge denominator (AnyPS5 counts only declared functions; do the same).
3. **TLS layout study**: how `main` lays out `thread_local` storage (static TLS via `tpidr_el0` offsets?) and what `nn::os` must allocate per thread. Document on switchbrew-level detail in `docs/dev/TLS.md`.
4. **NVN surface inventory**: the set of strings passed to `nvnDeviceGetProcAddress` across the corpus (static scan of `main` for `nvn*` string literals), ranked by frequency.
5. **Shader-translator spike**: run the vendored MIT translator over SASS blobs extracted from one title's romfs; confirm SPIR-V validates. Decide vendoring vs own implementation timeline.
6. **Legal checklist**: counsel review of D4/D5/D8/D9; written input contract; name chosen.
7. **Dependency snapshots**: mirror the MIT/0BSD/zlib projects listed in REUSE.md §13 off GitHub with license headers intact and record commit hashes in `3rdparty/PROVENANCE.md`; the upstreams are takedown targets (LibHac's repository and NuGet package were already removed).
Exit: a one-page "measured facts" note replacing section 2's "unverified" cells; go/no-go for Tier 1 host list.

### Phase 1 — Relinker and loader on Linux ARM64, 4 KiB pages (6–8 weeks)
1. NSO/NRO/NPDM parsers (port the parsing from Ghidra-Switch-Loader and reswitched `nxo64.py`, both ISC); ELF writer; `subsdk` conversion; TOML patches; `--verify-output`.
2. Patcher with all handlers in 4.2 (decoder tables generated from Arm's BSD-licensed AARCHMRS data; yuzu NCE read as reference only); `nx_svc` safety net with kernel semantics ported from SwitchGamesRecomp's runtime (MIT) and checked against libnx/Atmosphère.
3. **Synthetic conformance corpus**: small C/C++ programs built with Clang + devkitPro `elf2nso` against a *generated stub import library* of `sdk` names (from the Phase 0 inventory), exercising threads, mutexes, condvars, TLS, timers, files, and raw `svc`/`mrs`. These run on real hardware (for ground truth) and under AnySwitch; outputs must match. This corpus is fully redistributable and becomes CI.
4. Minimal `nx_init`, `nx_os` (threads, sync, TLS, tick, memory blocks), `libc_nx` (memory, string, stdio to host, math), `libcxx_nx` (ABI shims to host libc++ where layouts match; own implementation where they do not), `nx_diag` (logging/asserts).
Exit: every synthetic test passes; a libnx homebrew NRO (which uses raw `svc`) prints to the console via the safety net; first retail `main` reaches `nn::init::Start` and fails loudly on the first unimplemented `nn::*` import.

### Phase 2 — Native `sdk` core until first NVN call (8–12 weeks)
Start from the `nn::*` implementations already in SwitchGamesRecomp and LASRecomp (MIT) and the service semantics in Ryujinx's MIT HLE tree; names and prototypes from open-ead/nnheaders (reference only). Implement in Phase 0 frequency order: `nx_fs` (RomFS, save data with journaled commit, cache storage), `nx_oe`/applet (focus, operation mode, performance mode), `nx_hid` (Npad styles, touch, six-axis, vibration on SDL3; mapping file), `nx_time`, `nx_settings` (language/region), `nx_account` (single user), `nx_mem`, `nx_vi` (layer → window), `nx_prepo` (no-op), `nx_socket/nifm/ssl` (offline results), `nx_swkbd/err/web` (stubs listed in debt).
Tooling: `import_audit.py`, `progress.py`, `stubgen.py`, per-module `debt.md`, conventions checker, lavapipe CI.
Exit: three retail titles of different engines (one NintendoWare/`nn::gfx`, one Unity, one Unreal) run until their first `nvnBootstrapLoader` or `vk*` call with no unimplemented import before it. `import_audit` reports "absent = 0" for them.

### Phase 3 — Graphics: first frame, first playable (12–20 weeks)
1. **Passthrough path first**: titles shipping `NintendoSDK_Vulkan` `subsdk` (e.g. Hades) run on host Vulkan with their own SPIR-V. This yields a first *playable* title with no NVN work and validates audio/input/fs under real load.
2. NVN core: device/queue/memory pools/buffers/textures/samplers/programs/sync/window; command recording and replay; block-linear untiler; presentation through SDL3 + swapchain; frame pacing (59.94/60 Hz, operation-mode switching). Port the NVN-on-Vulkan layers from SwitchGamesRecomp and LASRecomp (both MIT) and the object model/texture layout from deko3d (zlib); untiler cross-checked against tegra_swizzle.
3. Shader recompiler: the MIT Ryujinx translator run out of process to get the first frame; native `core/shader/maxwell` (encodings from envytools tables and Mesa's GM107 emitter, nvdisasm as test oracle) developed in parallel against a growing SASS test corpus with per-instruction differential tests (host GPU vs reference values where measurable).
4. Caches: disk cache, pipeline journal, graphics pipeline library fast-link, background compile threads (lift the design straight from AnyPS5).
Exit: first NVN title renders its title screen; first 2D/simple NVN title is playable start to finish (AnyPS5's "Dreaming Sarah" moment). Public announcement happens here, not earlier.

### Phase 4 — Audio, dynamic modules, breadth (8–12 weeks)
`nx_audio` (`audren` command-list renderer: voices, mixes, biquads, effects, revision gating; `audout` queues — port Ryujinx.Audio, MIT, C# → C++), `nx_ro` (runtime NRO loading → in-process relink + `dlopen`), add-on content, `nn::pl` fonts (user-supplied or open substitutes), screenshots, controller rumble/motion, save-data import/export, per-title TOML patch library, blocker map.
Exit: ten titles playable across the three engine families; community bring-up threads open.

### Phase 5 — Hosts: macOS, Windows on ARM, Android (8–12 weeks, partly parallel with 4)
- macOS: Mach-O writer, 16 KiB page shim, `x18` handling per Phase 0 census (or Hypervisor.framework fallback), MoltenVK quirks, code signing/notarisation story for user-generated binaries.
- Windows on ARM: PE/ARM64 writer, entry stub resolving imports by name (AnyPS5's `WindowsEntryStubBuilder` design), TEB/`x18` policy.
- Android: optional; NDK build of the runtime, 16 KiB pages, storage access.
Exit: Phase 3's playable set runs on at least one Apple Silicon machine and one Windows-on-ARM machine.

### Phase 6 — Tier 2: x86-64 static recompiler (16–28 weeks)
1. Decoder + lifter to LLVM IR, built on dynarmic's A64 frontend (0BSD, azahar-emu fork) with ARMeilleure (MIT) and aslp spec-derived semantics (BSD-3) as oracles; recompiler/runtime structure from N64Recomp and XenonRecomp (MIT); jump-table micro-execution via Unicorn as a separate GPL tool; with the differential harness: every function in the synthetic corpus and in a title's `main` is executed natively on ARM64 (Tier 1) and recompiled on x86-64 with identical inputs; outputs and memory diffs must match.
2. Function discovery and jump tables (4.6); per-title TOML overrides for the residue.
3. Build-on-user-machine flow (Clang bundled or detected; cached objects); PE/ELF output on x86-64; the same runtime libraries built for x86-64.
4. Performance pass: register-state promotion to SSA, flag elimination, NEON vectorisation, hot-function profiles.
Exit: Phase 3's playable set runs on Linux and Windows x86-64 at ≥ console frame rate on a mid-range desktop GPU.

### Phase 7 — Community infrastructure (ongoing from Phase 3)
Progress badges and history, blocker map, WASM web converter (relinker only; Tier 2 needs a native compiler), prebuilt shader-cache packs, release packaging, PR overlap/labeling bots, GAME_TESTING guide. Copy AnyPS5's process wholesale: it is what turned a tool into a movement.

### Phase 8 — Switch 2 (gated; see section 9)

---

## 6. Work breakdown and sizing

| Component | New LOC (est.) | Comparable AnyPS5 measure | Phase |
|---|---|---|---|
| Relinker (parsers, writers, patches) | 8–15k | relinker 20k | 1 |
| Patcher + `nx_svc` | 3–6k | yuzu NCE patcher ~1k + handlers | 1 |
| Synthetic corpus + harness | 5–10k | core/libs/tests 87k incl. tests | 1 |
| `libc_nx` + `libcxx_nx` | 15–25k | libc 23k | 1–2 |
| `nx_os`, `nx_mem`, `nx_init`, `nx_diag`, `nx_time` | 10–15k | libkernel 27k | 1–2 |
| `nx_fs`, `nx_hid`, `nx_oe`, `nx_vi`, `nx_account`, `nx_settings`, stubs | 15–25k | ~20 small PRX libs | 2 |
| NVN → Vulkan (+ passthrough) | 30–60k | AgcDriver+Agc 86k (incl. PM4) | 3 |
| Maxwell shader recompiler | 25–45k | RDNA recompiler 71k | 3 |
| `nx_audio`, `nx_ro`, fonts, misc | 10–20k | Ngs2/Ajm/Font libs ~13k | 4 |
| macOS / WoA / Android host layers | 8–15k | PE/Mach-O patchers, allocator | 5 |
| Tier 2 recompiler + differential harness | 25–40k | (none; XenonRecomp ~15k) | 6 |
| Tooling (Python/JS) | 10–20k | tools 18k + web 2k | 2–7 |
| **Total to maturity** | **~165–300k** | AnyPS5 ≈ 300k + 87k tests | |

Team shapes:
- **Solo, part time:** Phase 0–3 (first playable, one ARM64 host) in 12–18 months. Tier 2 unrealistic without contributors.
- **Two full-time systems developers:** Phase 0–3 in 6–9 months; Phases 4–6 in a further 9–12 months.
- **Community after announcement (AnyPS5 pattern):** library breadth (Phases 2, 4) and per-title patches scale with contributors; the shader recompiler and Tier 2 remain core-team work.

---

## 7. First 30 days, concretely

1. Create the repository skeleton above, CONTRIBUTING (D5), input contract text (D4), mirror (D8).
2. Write `tools/priv_census.py` and `tools/symbol_inventory.py` (NSO parsing in Python; Capstone for decoding). Run over the corpus the team owns. Publish the aggregate numbers (no title identifiers needed).
3. Build the synthetic corpus toolchain: Clang targeting `aarch64-none-elf`, devkitPro `elf2nso`/`elf2nro`, generated stub `sdk` import library from the inventory. Run one test on real hardware via homebrew to pin ground truth.
4. Prototype the relinker NSO → ELF path and load a synthetic NSO under the host loader on an ARM64 Linux box (a Raspberry Pi 5 or an Ampere VM is enough).
5. Prototype the patcher on a synthetic NSO with deliberate `svc`/`mrs` instructions.
6. Vendored-translator spike on SASS blobs from one title.
7. Decide: Tier 1 host list, translator path, name. Update FEASIBILITY.md section 2 with measurements.

---

## 8. Open questions to settle by measurement (not by debate)

| # | Question | Settled by |
|---|---|---|
| Q1 | Does retail `main` contain raw `svc`, `mrs tpidr*`, or timer reads? How often? | Phase 0 census |
| Q2 | Is `x18` used as a GPR by game code or by the SDK-compiled code in `main`? | Phase 0 census |
| Q3 | How is `thread_local` storage laid out and sized for `main`, and who allocates it? | Phase 0 TLS study |
| Q4 | What fraction of the 21.5k `sdk` exports does the top-50 title set actually import? | Phase 0 inventory |
| Q5 | Which NVN entry points and object sizes do games depend on; do any touch object internals directly? | Phase 0 NVN inventory + Phase 3 assertions |
| Q6 | Does the MIT translator's SPIR-V run correctly across NVIDIA/AMD/Intel/Apple drivers, or do we need our own sooner? | Phase 0 spike, Phase 3 |
| Q7 | How many titles are 32/36-bit address space and need the low arena? | NPDM scan in Phase 0 |
| Q8 | Is compiling Tier 2 output on the user's machine acceptable (minutes, gigabytes), or must we emit object code directly? | Phase 6 prototype on one large title |

---

## 9. Switch 2 readiness (what to do now without any Switch 2 data)

Everything below is cheap if designed in from the start and expensive to retrofit:
1. **ISA level**: decoder/lifter/patcher must accept ARMv8.2+ (LSE atomics, FP16, dot-product, RCpc loads), ARMv8.3 pointer authentication (`pac*`/`aut*`/`xpac*`: treat as NOPs or strip under Tier 2; on Apple Silicon they execute natively), BTI landing pads, and `MTE` tags (ignore).
2. **Shader frontend**: `core/shader/maxwell` is built as `core/shader/<isa>` with a shared IR and backend; an SM86 (Ampere) decoder slots in later. Ampere SASS differs in instruction encoding, scheduling, and register files; nothing public exists yet.
3. **Graphics API**: keep the NVN object layer independent of NVN's function table so an NVN2 table can bind to the same objects where semantics overlap.
4. **Memory**: 12 GB guest arena, 16 KiB-friendly layout from the start (the T239's granule is unknown; Orin-class Linux uses 4 KiB but Horizon may differ).
5. **Policy**: no public claims about Switch 2 support; Phase 8 opens only when (a) decrypted Switch 2 binaries can be produced by owners from their own consoles, and (b) the input contract (D4) can be honoured without the project touching keys. Both are outside the project's control.

---

## 10. What "done" looks like for v0.1

A user on Linux ARM64 (and macOS if Phase 0 clears it) runs one command on a folder containing their decrypted `exefs` and `romfs`, gets a standalone application, and plays a Vulkan-passthrough title and one NVN title start to finish at native frame rate, with controller and audio working, saves persisting, and every unimplemented path failing loudly with a name they can paste into a bring-up thread. Progress badges show declared-function coverage per module. No key, no decryption step, no game data in any release artefact.
