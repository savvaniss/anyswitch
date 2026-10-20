# AnySwitch — Existing projects and how we use them

*Companion to [FEASIBILITY.md](FEASIBILITY.md) and [PLAN.md](PLAN.md). Licenses and activity checked against the repositories on 2026-10-20 (GitHub API where available). Re-verify before vendoring; takedowns are frequent in this space.*

## 0. How to read this

| Mode | Meaning |
|---|---|
| **Vendor** | Add as a git submodule under `3rdparty/` and link or call it. License must be compatible with ours. |
| **Port** | Translate the code (often C# → C++, Python → C++) into our tree, keeping its license header. License must be compatible. |
| **Tool** | Run as a separate executable at convert/build/test time. Its license does not flow into our binaries, so GPL tools are acceptable here. |
| **Reference** | Read it to learn semantics or layouts, then write our own. Never copy. Used for incompatible licenses (GPL-3 when we are MIT, or GPL-2-only), unlicensed code, and anything with provenance doubts. |
| **Corpus** | Freely redistributable test inputs. |
| **Avoid** | Do not use, link, cite in docs, or mirror. |
| **Collaborate** | Upstream/downstream relationship worth proposing. |

**License policy (PLAN.md decision D6, revised by this document):** the project is **MIT**. Reason: every reusable Switch-side implementation in 2026 is MIT or more permissive (Ryujinx lineage, LAS Recomp family, SwitchGamesRecomp, N64Recomp/XenonRecomp, dynarmic 0BSD, envytools, Mesa, tegra_swizzle, SDL, LLVM, SPIRV-Tools), while the GPL pieces (yuzu/Eden lineage, ssbu-recomp, exlaunch, Atmosphère, AnyPS5 itself) are either takedown-exposed or useful only as references. MIT also lets the per-game recomp projects pull our runtime back, which is how a generic tool earns contributors. The cost is that AnyPS5's GPL-2.0-only *code* cannot be copied in; only its design can. Apache-2.0 dependencies (SPIRV-Tools, SPIRV-Cross, MoltenVK, Remill, Ghidra) are fine under MIT and would not have been under GPL-2.0-only.

---

## 1. Executable formats, relinker, loader (Phase 1)

| Project | License | Status 2026-10-20 | Mode | What we take |
|---|---|---|---|---|
| switchbrew.org wiki (NSO, NRO, MOD, Rtld, NPDM, SVC, Thread Local Region, Memory layout) | wiki content | live | Reference | The format and ABI specification. Primary source for everything in `core/relinker` and `nx_svc`. |
| [switchbrew/libnx](https://github.com/switchbrew/libnx) | ISC | active (pushed 2026-10-19), 1.5k★ | Port + Reference | Kernel struct layouts, SVC wrappers and numbers, TLS/thread-context conventions, `nn`-style service semantics as seen from the client side (hid shared memory, fs, audio, applet, nvdrv ioctls we do *not* need). ISC is MIT-compatible. |
| [switchbrew/switch-tools](https://github.com/switchbrew/switch-tools) (`elf2nso`, `elf2nro`, `npdmtool`, `nacptool`) | ISC | last push 2024-10, 181★ | Tool | Build the synthetic conformance corpus (Phase 1.3) and homebrew test inputs. |
| [switchbrew/switch-examples](https://github.com/switchbrew/switch-examples), nx-hbmenu, SDL2 homebrew | ISC-family (check per file) | active (2026-02) | Corpus | Redistributable NROs that exercise `svc`, hid, fs, audio through libnx; first loader targets. |
| [Adubbz/Ghidra-Switch-Loader](https://github.com/Adubbz/Ghidra-Switch-Loader) | ISC | active (2026-09), 374★ | Port (parsing) + Tool | NSO/NRO/KIP/MOD0 parsing and ELF reconstruction logic (Java → C++); also the analysis tool for Phase 0 censuses. |
| [reswitched/loaders](https://github.com/reswitched/loaders) (`nxo64.py`) | ISC | dormant (2020) | Port (Python tooling) | NSO/NRO parsing, dynamic section and relocation walking for `tools/priv_census.py`, `tools/symbol_inventory.py`. |
| [shuffle2/nx2elf](https://github.com/shuffle2/nx2elf) | **no license** | pushed 2026-04, 112★ | Reference | The canonical NSO → ELF reconstruction; proves the approach, cannot be copied. |
| Thog/oss-rtld | ISC | archived 2026-01, URL moved | Reference | rtld semantics (module discovery, relocation order, symbol lookup order) for our loader notes. |
| [Atmosphere-NX/Atmosphere](https://github.com/Atmosphere-NX/Atmosphere) (libmesosphere, libstratosphere) | GPL-2.0 | active (2026-10), 20k★ | Reference | SVC ABI (register clobbers, return conventions), kernel object semantics (arbitration, condvars, events, address waits), and `ams::os` as a clean-room model of `nn::os`. GPL-2.0 is incompatible with MIT: read, do not copy. |
| [lz4](https://github.com/lz4/lz4), [zstd](https://github.com/facebook/zstd) | BSD-2 / BSD-3 | active | Vendor | NSO segment decompression (LZ4; zstd for 22.0.0+ "Zbic"). |
| [marzer/tomlplusplus](https://github.com/marzer/tomlplusplus) | MIT | active (2026-07) | Vendor | Title patch files (AnyPS5's strict-TOML idea). |
| LLVM `lld` / own writers | Apache-2.0 + LLVM exception | active | Reference | Mach-O and PE/ARM64 writer details; AnyPS5 wrote its own writers and so will we, to keep the relinker dependency-free and WASM-buildable. |

## 2. Privileged-instruction patcher and Tier 1 execution (Phase 1)

| Project | License | Status | Mode | What we take |
|---|---|---|---|---|
| yuzu NCE (`src/core/arm/nce/`) as preserved in Eden (git.eden-emu.dev) and Citron mirrors | GPL-2.0-or-later files / GPL-3 project | Eden active (2026-10-19), GitHub copies removed | Reference | The patch-class list (`svc`, `mrs tpidrro_el0/tpidr_el0`, `msr tpidr_el0`, `mrs cntpct_el0`, exclusives), the ±128 MiB patch-section trick, `TlsMagic` discrimination in signal handlers, the 16 KiB-page rework (Eden issue #3607 / PR #4181). Read only. |
| Ryujinx `Ryujinx.Cpu/AppleHv` (MIT trees: [NextendoNetwork/Ryujinx-Nextendo](https://github.com/NextendoNetwork/Ryujinx-Nextendo) `src/`, Ryubing self-hosted) | MIT (upstream code) | Nextendo fork active (2026-10-20) | Port (optional) | Hypervisor.framework fallback for macOS if the Phase 0 `x18`/TLS census rules out in-process patching there: EL1 vector shim, `hv_vcpu` loop, trapped `cntfrq`/`cntpct`. |
| Arm **AARCHMRS_BSD** (A-profile Machine Readable Specification, `Instructions.json`, `Registers.json`; mirror: kernel.googlesource.com/…/maz/AARCHMRS, 2026-09 drop) | BSD-3-Clause (Arm Limited) | current | Vendor (data) | Generate the AArch64 decoder tables for the patcher scanner and the Tier 2 lifter from Arm's own machine-readable encodings instead of hand-written tables. |
| [capstone-engine/capstone](https://github.com/capstone-engine/capstone) | BSD-3 | active (2026-10), 9.1k★ | Vendor (prototyping) | Fast disassembler for Phase 0 tools and early patcher; replace with generated tables later to drop the dependency from the WASM relinker. |

## 3. Native `sdk` runtime: `nx_*`, `libc_nx`, `libcxx_nx` (Phases 1–2, 4)

| Project | License | Status | Mode | What we take |
|---|---|---|---|---|
| [BelmanteGu/SuperMarioWonderRecomp](https://github.com/BelmanteGu/SuperMarioWonderRecomp) (`runtime/`: kernel subset, HIPC parser, `nn::*` HLE registry by name, memory, platform layers for Windows/macOS/Android) | MIT | active (2026-10-18), 331★ | Port + **Collaborate** | The closest existing generic runtime. Port the kernel/sync primitives (ArbitrateLock/Unlock, WaitProcessWideKeyAtomic, WaitForAddress, events, QueryMemory, 19.2 MHz tick), the by-name HLE registry design, and the `nn::os`/`nn::fs`/`nn::hid`/`nn::oe` implementations that exist. **Do not** port its importer: it takes `prod.keys`/`title.keys` (violates D4). Propose a shared-runtime split upstream. |
| [peiikai/LASRecomp](https://github.com/peiikai/LASRecomp) runtime (and [Apolinario-coder/SkyRecomp](https://github.com/Apolinario-coder/SkyRecomp), letsgo-recomp, AstralChain-PC which reuse it) | MIT (own code) | active (2026-10-20), 1.4k★ | Port + Collaborate | Kernel/thread/condvar handling, task forwarder, memory layout for 16 KiB hosts, start-screen "compile on the player's machine" flow, input mapping. Second opinion on every `nn::os` edge case. |
| Ryujinx `Ryujinx.HLE/HOS/Services` (`Account`, `Am`/applet, `Fs`, `Hid`, `Nifm`, `Ns`, `Pctl`, `Prepo`, `Settings`, `Time`, `Friend`, `Bcat`, `Ro`, `Sm`) in MIT trees | MIT | see above | Port (semantics, not IPC) | Behavioural reference with permissive license for what each `nn::*` call must return (error codes, user model, applet messages, operation/performance mode, time zones, language/region settings, parental-control defaults). We implement at the `nn::*` C++ ABI, so ports are semantic, not structural. |
| [open-ead/nnheaders](https://github.com/open-ead/nnheaders) (fork of ultimate-research/nnheaders; from unstripped Splatoon 2 / Pokémon SwSh / OdysseyReversed) | **no license**; "does not accept copyrighted material" | active (2026-10-11), 50★ | Reference (names/prototypes only) | Mangled names, parameter lists, enum values, struct *sizes*. Re-derive layouts into our own headers with a source note per type. Provenance is public-binary RE, acceptable for names; keep it out of `3rdparty/`. |
| [ultimate-research/nnsdk-rs](https://github.com/ultimate-research/nnsdk-rs) | no license file | active (2026-09) | Reference | Same as above, Rust bindings; useful cross-check of export names by namespace. |
| LibHakkun (codeberg.org/fruityloops1/LibHakkun) symbol files; [skyline-dev/skyline](https://github.com/skyline-dev/skyline) (MIT); [shadowninja108/exlaunch](https://github.com/shadowninja108/exlaunch) (GPL-2.0); SaltyNX (no license) | mixed | active | Reference | Evidence of which `sdk` exports games bind and how hooks replace them (including `nvnBootstrapLoader`). Skyline (MIT) may be ported for its `nn` hook declarations. |
| [musl](https://musl.libc.org) | MIT | active | Port | `libc_nx`: Horizon's libc *is* musl. Port the string/stdio/math/locale surface and `errno` model, backed by our guest heap; keep host libc for syscalls. |
| LLVM libc++ (host) | Apache-2.0 + LLVM exception | active | Vendor (host) | `libcxx_nx`: Itanium ABI matches; forward where layouts match (`std::string`, `std::vector`), reimplement where Horizon's libc++ version differs. |
| [microsoft/mimalloc](https://github.com/microsoft/mimalloc) | MIT | active (2026-10), 13k★ | Vendor | Guest heap arena allocator (AnyPS5 uses it on Windows). |
| [Denkishi/ssbu-recomp](https://github.com/Denkishi/ssbu-recomp) (`nn*.c`, ~1,100 entry points) and B2recomp | GPL-3.0 | active (2026-10-20) | Reference | Which `nn::*` functions a very large title needs; GPL-3 is incompatible with MIT, so read only. |
| AnyPS5 `libkernel`, `libc`, thread-cancellation and exception designs | GPL-2.0-only | active | Reference | Design patterns only (guest arena, write-watch, `NotImplemented` policy, debt files). Incompatible with MIT. |

## 4. Filesystem, save data, content (Phase 2)

| Project | License | Status | Mode | What we take |
|---|---|---|---|---|
| **LibHac** (thealexbarney/libhac) | MIT | **GitHub repository returns 404 and NuGet hides the package "as it violates our Terms of Use" (checked 2026-10-20)** | Reference, via copies in MIT Ryujinx trees and the Rust port [DCNick3/hac-rs](https://github.com/DCNick3/hac-rs) | RomFS directory/file tables, save-data journal/duplex layout, partition FS. Use only the plaintext-format parts; LibHac's crypto/NCA code is exactly what got it removed and is forbidden by D4. |
| switchbrew Filesystem services, Savegames pages | wiki | live | Reference | Save data commit semantics, cache storage, add-on content mounts. |
| Ryujinx `Ryujinx.HLE/FileSystem` | MIT | see §2 | Reference | How titles mount RomFS/save/AOC and what `nn::fs` error codes they expect. |

## 5. Input (Phase 2)

| Project | License | Status | Mode | What we take |
|---|---|---|---|---|
| [libsdl-org/SDL](https://github.com/libsdl-org/SDL) (SDL3) | Zlib | active (2026-10), 16.8k★ | Vendor | Window, Vulkan surface, gamepad (incl. Joy-Con/Pro Controller HIDAPI drivers, rumble, gyro), keyboard/mouse, audio output. AnyPS5 uses SDL2 the same way. |
| libnx `hid` headers; Ryujinx `Hid` service; switchbrew HID shared memory | ISC / MIT / wiki | active | Port (layouts) + Reference | Npad style sets, LIFO ring layouts, six-axis and touch structs in case a title reads shared memory directly. |

## 6. Audio (Phase 4)

| Project | License | Status | Mode | What we take |
|---|---|---|---|---|
| Ryujinx `Ryujinx.Audio` (`Renderer/`: `audren` command-list renderer with voices, mixes, splitters, sinks, effects (reverb, delay, biquad, aux, limiter, compressor), ADPCM/PCM decoders, revision gating; `audout` queues) | MIT | see §2 | **Port (C# → C++)** | The whole `nn::audio` renderer is already implemented permissively. This is the single biggest reuse win after the shader translator. |
| yuzu/Eden `audio_core` | GPL | Eden active | Reference | Cross-check only. |
| [miniaudio](https://github.com/mackron/miniaudio) | MIT-0 / public domain | active | Vendor (optional) | Low-latency backend where SDL3 audio is not enough. |

## 7. NVN graphics layer (Phase 3)

| Project | License | Status | Mode | What we take |
|---|---|---|---|---|
| [devkitPro/deko3d](https://github.com/devkitPro/deko3d) | Zlib | active (2026-10-17), 418★ | Port + Reference | NVN object model (device, queue, memory block/pool, command buffer recording, sync, swapchain), texture layout and swizzle code (`dk_texture`), descriptor set conventions. Zlib is MIT-compatible. |
| SuperMarioWonderRecomp `runtime/src/nvn/` + `docs/NVN.md`, `docs/nvn/TILING.md`, `SHADERS.md`, 67 documented rendering bugs | MIT | active | **Port + Collaborate** | NVN → Vulkan replay design, `VK_EXT_external_memory_host` pool import, compute-shader untiler, ASTC decode on desktop, shadow buffers + write-protect tracking (Android), extended dynamic state usage. Propose extracting it as a shared library both projects consume. |
| LASRecomp NVN layer + Metal/MetalFX backend (reused by SkyRecomp, letsgo-recomp, AstralChain-PC) | MIT | active | Port | Second NVN implementation to diff against; the Metal backend is the path to a native macOS renderer later. |
| Ryujinx `Ryujinx.Graphics.Texture` (block-linear swizzle, format tables, ASTC/BC decoders), `Ryujinx.Graphics.Gpu` state semantics | MIT | see §2 | Port | Texture layout math and format conversion tables; Maxwell state semantics where NVN maps 1:1 (blend, depth, stencil, viewport, scissor). |
| [ScanMountGoat/tegra_swizzle](https://github.com/ScanMountGoat/tegra_swizzle) | MIT | active (2026-07) | Port (Rust → C++) + Tool | Reference swizzle/deswizzle with tests; good differential oracle for our untiler. |
| [mawmz/imgui-xeno](https://github.com/mawmz/imgui-xeno) (renders ImGui through NVN inside games), SaltyNX (`nvnBootstrapLoader` replacement) | GPL-2.0 / none | active | Reference, **check provenance** | Real-world NVN call sequences and object sizes. Any NVN header in these repos may derive from the SDK; treat the headers as off-limits and only learn from the call patterns. |
| Vulkan-Headers, Vulkan-Loader, [volk](https://github.com/zeux/volk), [VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) | Apache-2.0 / MIT / MIT | active | Vendor | Standard Vulkan stack. |
| [KhronosGroup/MoltenVK](https://github.com/KhronosGroup/MoltenVK) | Apache-2.0 | active (2026-10) | System ICD (not linked) | macOS until a Metal backend exists; note Ryujinx's finding that MoltenVK is sync-heavy. |
| AnyPS5 `libSceAgcDriver` (pipeline library fast-link + background LTO relink, pipeline journal replay, shader disk cache with versioned entries, host-import budgets, write-watch) | GPL-2.0-only | active | Reference | The cache/pipeline architecture to reproduce. Design only. |

## 8. Maxwell shader recompiler (Phase 3)

| Project | License | Status | Mode | What we take |
|---|---|---|---|---|
| Ryujinx `Ryujinx.Graphics.Shader` (Maxwell decode → SSA IR → GLSL/SPIR-V; MSL via SPIRV-Cross) in MIT trees | MIT | fork trees active | **Tool first (offline, .NET), then Port to C++** | A complete SM5x → SPIR-V translator. SwitchGamesRecomp already runs it offline over 12,717 shader blocks. Vendor a snapshot into our own mirror immediately; the upstream forks are takedown targets. |
| LASRecomp shader translator (built from envytools docs) | MIT | active | Port / Reference | Independent SASS decoder to cross-check encodings. |
| [envytools/envytools](https://github.com/envytools/envytools) (`envydis` gm107 tables, hardware docs) | MIT | dormant (2023), 546★ | Vendor (tables) + Reference | Machine-readable Maxwell instruction encodings for our decoder; documentation of the shader program header and scheduling words. |
| Mesa `src/nouveau/codegen` (GM107 emitter) and NVK compiler | MIT | active | Reference | Encoder tables are a second source of truth for instruction formats and operand semantics. |
| NVIDIA `nvdisasm` (CUDA toolkit, `-b SM53`) | proprietary, freely usable | current | Tool (test oracle only) | Differential disassembly tests for the decoder; never redistributed. |
| switchbrew "GPU Shaders" page; NVIDIA open-gpu-doc Shader Program Header | wiki / NVIDIA public doc | live | Reference | Shader header layout. |
| [KhronosGroup/SPIRV-Tools](https://github.com/KhronosGroup/SPIRV-Tools), [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross), SPIRV-Headers | Apache-2.0 | active | Vendor | Validation/optimisation of emitted SPIR-V; MSL generation for a future Metal path. Allowed under MIT (would not be under GPL-2.0-only). |
| yuzu/Eden `shader_recompiler` | GPL | Eden active | Reference | Cross-check of instruction semantics only. |
| deko3d's `uam` shader compiler (Mesa-based) | NOASSERTION (devkitPro) | dormant (2022) | Reference | How a Maxwell shader binary is assembled for NVN-style consumption. |

## 9. Tier 2 static recompiler, AArch64 → x86-64 (Phase 6)

| Project | License | Status | Mode | What we take |
|---|---|---|---|---|
| **dynarmic** ([azahar-emu/dynarmic](https://github.com/azahar-emu/dynarmic) maintained fork; original merryhime repo gone; lioncash fork archived 2024) | 0BSD | active (2026-09-26) | **Port (A64 frontend)** | A complete, permissively licensed C++ AArch64 decoder plus instruction-to-IR semantics (`frontend/A64`). Emit its IR to LLVM IR instead of to a JIT. Also its test suite as a semantics oracle. |
| Ryujinx `ARMeilleure` (A64 decoder + instruction semantics, C#) | MIT | fork trees active | Reference / Port | Second complete A64 semantics implementation; useful where dynarmic lacks an instruction (newer extensions for Switch 2 readiness). |
| Arm AARCHMRS_BSD `Instructions.json` | BSD-3 | current | Vendor (data) | Generated decoder tables; feature gating per ISA level. |
| [UQ-PAC/aslp](https://github.com/UQ-PAC/aslp) (partial evaluator for Arm ASL; offline lifter generator) | BSD-3 | active (2026-06) | Tool | Spec-derived per-instruction semantics for differential testing of the lifter, and a candidate generator for instructions dynarmic does not cover. |
| [N64Recomp/N64Recomp](https://github.com/N64Recomp/N64Recomp) | MIT | active (2026-05), 8.3k★ | Port (structure) | Recompiler/runtime split, per-function C emission, `LOOKUP_FUNC` indirect-call table, TOML configuration of overlays and patches, the "user compiles" workflow. |
| [hedge-dev/XenonRecomp](https://github.com/hedge-dev/XenonRecomp) (+ ReXGlue) | MIT | last push 2025-08, 6.6k★ | Port (structure) | Jump-table discovery passes, perfect-hash indirect dispatch, `.pdata`-style function-bounds input (we have `.eh_frame_hdr`), per-title TOML overrides. |
| SuperMarioWonderRecomp `recomp/` (Python AArch64 → C++) and LASRecomp `tools/` (Capstone + Unicorn micro-execution for jump tables) | MIT | active | Port + Collaborate | The only AArch64-specific recompilers with shipped games: jump-table idioms, function discovery heuristics, the measured size problem (1.4 GB C++), build-on-user-machine flow. |
| [unicorn-engine/unicorn](https://github.com/unicorn-engine/unicorn) | GPL-2.0 | active (2026-10), 9.5k★ | Tool (separate process) | Micro-execution to resolve ambiguous jump tables at convert time. GPL is fine for an out-of-process tool; never link it into MIT binaries. |
| LLVM (IR builder, `clang`, `lld`) | Apache-2.0 + LLVM exception | active | Vendor / Tool | Lifter backend and the user-side compiler. |
| [ziglang/zig](https://github.com/ziglang/zig) (`zig cc`) | MIT | active | Tool | Portable, single-download Clang+LLD for compiling recompiled output on the player's machine (LAS Recomp's approach). |
| [lifting-bits/remill](https://github.com/lifting-bits/remill) | Apache-2.0 | active (2026-08), 1.9k★ | Reference / optional Vendor | Alternative AArch64 → LLVM lifter with runtime-state modelling; compare its memory/flag model with ours. |
| Ghidra SLEIGH AArch64 spec ([NationalSecurityAgency/ghidra](https://github.com/NationalSecurityAgency/ghidra)) | Apache-2.0 | active | Reference / Tool | p-code semantics as a third oracle; Ghidra + Switch loader for analysis. |
| Suyu 0.0.4 AOT "Recompiler mode" | GPL-3 | archived 2026-09 | Reference | The only generic AArch64 → x86-64 AOT stage for Switch titles; note its design decisions, read only. |
| FEX-Emu, Box64 | MIT | active | Not applicable | Opposite direction (x86 guest on ARM host). Only their 16 KiB-page handling (Box64, Aug 2026) is relevant, for Tier 1 on Asahi. |

## 10. Tooling, QA, community (Phases 2–7)

| Project | License | Status | Mode | What we take |
|---|---|---|---|---|
| AnyPS5 `tools/` (`import_audit.py`, `progress.py`, `stubgen.py`, `check_conventions.py`, `pr_overlap.py`, `env_reference.py`, `package_release.py`), `docs/dev/debt/*.md`, `GAME_TESTING.md`, blocker map, bring-up discussions, WASM web converter | GPL-2.0-only | active (12k commits) | Reference (rewrite) + **Collaborate** | The community process that made AnyPS5 work. Rewrite the scripts (they are small) rather than copy, or ask the author about dual-licensing the tooling. Propose cross-linking the two projects. |
| Emscripten | MIT | active | Tool | Browser build of the relinker (Phase 7). |
| Mesa lavapipe | MIT | active | Tool (CI) | Headless Vulkan for graphics tests in CI, as AnyPS5 does. |
| Catch2 / GoogleTest | BSL-1.0 / BSD-3 | active | Vendor | Unit and conformance tests. |

## 11. Avoid entirely

| Project / material | Why |
|---|---|
| hactool, hactoolnet, nxdumptool, Lockpick_RCM, any key-dumping or decryption tool | Decryption and dumping are outside the input contract (PLAN.md D4). Never reference them in code, docs, issues, or READMEs. LibHac's removal shows that handling keys is enough to be targeted. |
| Leaked Nintendo SDK or NVN headers, or any file whose provenance is the official SDK (including such headers found inside modding repos) | Poisons the clean-room argument and is copyrighted. |
| yuzu / Eden / Citron / Sudachi / Suyu / Kenji-NX / Ryubing *emulator* code paths that decrypt or load keys | Both GPL (yuzu lineage) and the subject of the February and August 2026 takedowns. Only the MIT Ryujinx subsystems listed above (audio, shader, texture, HLE service semantics, ARMeilleure, AppleHv) are usable, and only from trees that keep the MIT headers. |
| HWDER's `.nsp` + `prod.keys` import flow | Functionally identical to the emulators' decryption step. |
| nx2elf code | No license. |

## 12. Collaboration shortlist

1. **BelmanteGu (SwitchGamesRecomp)** — propose a shared, versioned "nx runtime" package (kernel subset, `nn::*` HLE, NVN layer) that both a per-game recomp and a generic relinker consume; they already serve two games from one runtime.
2. **peiikai (LASRecomp) and the SkyRecomp / letsgo / AstralChain forks** — same proposal from the other direction; their NVN and Metal layers are the most battle-tested on macOS.
3. **boykopovar (AnyPS5)** — cross-pollinate tooling and community process; ask about MIT-licensing the small Python tools so both projects can share them.
4. **Eden maintainers** — the 16 KiB-page NCE rework is the closest work on the exact Tier 1 problem; knowledge exchange only (GPL).
5. **Ryubing / Kenji-NX maintainers** — agree on a canonical MIT snapshot of `Ryujinx.Graphics.Shader` and `Ryujinx.Audio` to mirror, so the dependency survives further takedowns.

## 13. Phase-0 action items this document adds

- Create a `3rdparty-mirrors` organisation/namespace off GitHub and snapshot, with license files intact: Ryujinx (MIT tree), SuperMarioWonderRecomp, LASRecomp, azahar-emu/dynarmic, envytools, deko3d, tegra_swizzle, N64Recomp, XenonRecomp.
- Record for each vendored snapshot the commit hash, the upstream URL at the time, and the license header check in `3rdparty/PROVENANCE.md`.
- Write the provenance rule into CONTRIBUTING: every struct layout or constant in `core/libs/nx` carries a comment naming its public source (switchbrew page, libnx header, measured behaviour), and PRs that introduce an unexplained layout are rejected (AnyPS5's rule).
