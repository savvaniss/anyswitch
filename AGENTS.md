# AGENTS.md

Instructions for AI coding agents (Claude Code, Codex, Copilot, Cursor, Gemini and others) and for the humans directing them. Human contributors should read [CONTRIBUTING.md](CONTRIBUTING.md) too; everything here applies to them as well.

This file is the contract. If a task conflicts with a rule below, stop and say so in the pull request or issue instead of working around the rule.

## 1. What this project is

anyswitch converts Switch executables that a user already has in plain form (NSO/NRO files from an extracted `exefs`) into native host programs, and supplies a native runtime (`libnxrt.so`) in place of the title's `sdk` module. Nothing is emulated: on AArch64 hosts the original machine code runs in-process after a small set of instructions is patched. The design is in [docs/dev/ARCHITECTURE.md](docs/dev/ARCHITECTURE.md); the roadmap is [docs/PLAN.md](docs/PLAN.md).

Read these before changing code, in this order:

1. This file.
2. [docs/dev/ARCHITECTURE.md](docs/dev/ARCHITECTURE.md): layout of relinked modules and the relinker/runtime contract.
3. [docs/dev/CONVENTIONS.md](docs/dev/CONVENTIONS.md): code style and naming.
4. [docs/dev/RECIPES.md](docs/dev/RECIPES.md): step-by-step instructions for the common tasks.
5. The debt file for the module you touch: `docs/dev/debt/<module>.md`.

## 2. Hard rules: legal and provenance (never break these)

- **L1. No keys, no decryption, no dumping.** Do not write code, tests, scripts, docs, comments, issue text or commit messages that read, derive, store, request or use cryptographic keys or title keys, decrypt anything, unpack packaged or encrypted containers (NSP, XCI, NCA and similar), or explain how to obtain game files or keys. Do not name tools that do those things anywhere outside the policy documents that already exist. The relinker refuses packaged input on purpose; keep it that way.
- **L2. No game data in the repository.** Never commit or attach executables, assets, saves, firmware, system fonts, shader binaries, symbol dumps or screenshots taken from a commercial title, nor any file derived from one (relinked output, census output that names a title, extracted strings). Tests use only the synthetic corpus in `tests/corpus`, which is built from source.
- **L3. No SDK material.** Never use, paste or paraphrase leaked SDK or NVN headers, documentation or source, or code whose provenance you cannot state. If you remember an SDK detail and cannot cite a public source for it, do not use it.
- **L4. Cite public sources.** Every structure layout, constant, result code and behaviour you implement carries a short comment naming its public source: a switchbrew page, an ISC/MIT/BSD-licensed project file (libnx, deko3d, Ryujinx MIT tree, envytools, Mesa), an Arm or NVIDIA public document, or "measured: <how>" for behaviour observed with the synthetic corpus on real hardware. Names and prototypes may come from public symbol reconstructions (open-ead/nnheaders, nnsdk-rs); layouts must still be justified independently.
- **L5. License hygiene.** First-party code is MIT. Never copy code from GPL projects (yuzu and its forks, Eden, Citron, Suyu, Atmosphère, ssbu-recomp, AnyPS5, Unicorn) or from unlicensed projects (nx2elf, nnheaders): read them for understanding only. Code from permissive projects may be ported with its license header kept and an entry in [3rdparty/PROVENANCE.md](3rdparty/PROVENANCE.md). Third-party code enters only as a git submodule under `3rdparty/`. [docs/REUSE.md](docs/REUSE.md) says which mode applies to each known project.
- **L6. No title-specific behaviour in the runtime.** Never branch on a program ID, module ID, title name or build ID in `runtime/` or `core/`. Title-specific fixes belong in a future patch-file mechanism, not in code.

## 3. Hard rules: engineering

- **E1. Fail loud.** Unimplemented or unexpected states stop the process with a message that names the function or SVC, using `NX_UNIMPLEMENTED(...)` or `nxrt::fatal(...)` in the runtime and `nx::fail(...)` in host tools. Never return a fake success, silently ignore an argument, or stub a function to "make progress", unless the function only affects presentation (for example telemetry or a UI dialog) and the stub is listed in its debt file.
- **E2. The contract is one header.** The relinker and the runtime communicate only through `core/abi/include/nxabi/nxabi.h`. Any layout or meaning change bumps `NXABI_VERSION` and updates the "Relinker/runtime contract" section of ARCHITECTURE.md in the same pull request.
- **E3. Do not emulate the kernel's IPC.** The runtime replaces the SDK at its function boundary. Do not add handling for `SendSyncRequest*` SVCs or reimplement Horizon services behind IPC. If a title needs one, report it as a finding.
- **E4. Every guest export is tested through a real prototype.** A new `NX_GUEST_FN` needs a declaration in `tests/corpus/include/` and a corpus case that calls it, so the mangled name is checked by the compiler. A typo in a mangled name otherwise only shows up in a real title.
- **E5. Record what you skip.** If an implementation is partial, add a bullet to `docs/dev/debt/<module>.md` in the same pull request: what is missing, what the console does (with source), and how it fails today.
- **E6. Host tools stay portable and dependency-free.** `core/` builds with GCC, Clang and MSVC in C++20 using only the standard library. The runtime may use POSIX and Linux APIs.
- **E7. Keep the corpus green.** `tools/dev/test.sh` (unit tests plus every corpus case) must pass before you open a pull request. If you change behaviour on purpose, update the expectation in `tests/corpus/cases.json` and say why in the pull request.

## 4. Repository map

| Path | What lives there |
|---|---|
| `core/abi/` | `nxabi.h`, the relinker/runtime contract |
| `core/common/` | byte readers/writers, file IO, SHA-256, errors |
| `core/format/` | NSO, NRO, NPDM, MOD0 and ELF parsing; LZ4; input-contract checks |
| `core/relinker/` | AArch64 encoders, patch scanner, census, ELF writer, `anyswitch-relink` CLI |
| `runtime/src/rt/` | runtime core: entry, module discovery, guest threads, SVC safety net, traps, crash reports |
| `runtime/src/<module>/` | guest API modules (`nn_os`, `libc`, `cxxabi`, ...), one folder per SDK area |
| `tests/unit/` | host unit tests (no dependencies) |
| `tests/corpus/` | synthetic guest programs, their headers, linker script and `cases.json` |
| `tools/dev/` | `build.sh`, `test.sh` |
| `tools/corpus/` | corpus builder and runner |
| `tools/phase0/` | measurement tools for users' own titles (aggregate output only) |
| `tools/bootstrap/` | rootless toolchain download (Zig, qemu-user, AArch64 sysroot) |
| `tools/site/`, `site/` | GitHub Pages: progress numbers, badges, roadmap map, home and progress pages |
| `docs/roadmap.json` | milestones and their status; the badges and progress page are generated from it |
| `docs/dev/debt/` | one file per module listing known gaps |
| `.github/` | CI, trusted-base conventions check, Pages, progress comments, releases, labels, templates |

## 5. Build and test

```bash
tools/bootstrap/fetch_toolchain.sh     # once: Zig, qemu-user and an AArch64 sysroot, no root needed
tools/dev/build.sh                     # host tools and libnxrt.so into build/
tools/dev/test.sh                      # unit tests and every corpus case
tools/dev/test.sh build threads        # one corpus case
python3 -I tools/check_conventions.py  # the checks CI runs on every pull request
```

Windows hosts can build and unit-test the host tools natively (MinGW or MSVC); the runtime and corpus need Linux, WSL or macOS with the toolchain above. Details: [docs/dev/BUILD.md](docs/dev/BUILD.md) and [docs/dev/TESTING.md](docs/dev/TESTING.md).

## 6. How to work on a task

1. **Scope.** One module or one concern per pull request. Do not reformat files you did not otherwise change. Do not add dependencies.
2. **Find the gap first.** For a missing guest function, run `anyswitch-relink provided --demangle build/runtime/libnxrt.so` and read the module's debt file before writing anything.
3. **Source the semantics.** Find the public source for the behaviour (rule L4) before coding. If none exists, write a corpus case that documents the assumption, mark it as an assumption in the debt file, and say so in the pull request.
4. **Follow the recipe** in [docs/dev/RECIPES.md](docs/dev/RECIPES.md) for adding a guest function, an SVC, a corpus case, a patch class or an ABI change.
5. **Test.** Run `tools/dev/test.sh` and `tools/check_conventions.py`. Paste the summary lines into the pull request.
6. **Update the roadmap.** If the change completes or starts a milestone in `docs/roadmap.json`, change its status in the same pull request. A bot comments the effect on the progress numbers.
7. **Describe.** Use the pull request template. List sources, debt changes and anything you could not verify.

## 7. Style for agents

- Prefer small, reviewable diffs. Explain non-obvious decisions in the pull request, not in long code comments.
- Comments in code are for sources (rule L4) and for "why", never for narrating the change.
- Do not invent APIs, mangled names, error codes or struct sizes. If you are unsure, say so.
- When a command fails, report the output; do not paper over it by loosening a test.
- Commit messages follow Conventional Commits with the module as scope: `feat(nn_os): add event objects`, `fix(relinker): keep weak imports symbolic`.

## 8. Things that look helpful but are not allowed

- "Supporting" packaged formats because a user asked for convenience (rule L1).
- Adding a title's import list, crash log or census output to the repository as a test fixture (rule L2).
- Copying a function from an emulator "because the semantics are the same" (rule L5).
- Returning success from an unimplemented function so a title gets further (rule E1).
- Hard-coding a workaround for one game in the runtime (rule L6).
