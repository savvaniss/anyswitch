# anyswitch

[![ci](https://github.com/savvaniss/anyswitch/actions/workflows/ci.yml/badge.svg)](https://github.com/savvaniss/anyswitch/actions/workflows/ci.yml) [![SDK exports](https://savvaniss.github.io/anyswitch/badge-exports.svg)](https://savvaniss.github.io/anyswitch/progress.html#sdk) [![corpus](https://savvaniss.github.io/anyswitch/badge-corpus.svg)](https://savvaniss.github.io/anyswitch/progress.html#corpus) [![roadmap](https://savvaniss.github.io/anyswitch/badge-roadmap.svg)](https://savvaniss.github.io/anyswitch/progress.html#roadmap)

[![roadmap map](https://savvaniss.github.io/anyswitch/progress.svg)](https://savvaniss.github.io/anyswitch/progress.html)

Website and progress: [savvaniss.github.io/anyswitch](https://savvaniss.github.io/anyswitch/) · Title bring-up reports: [Discussions](https://github.com/savvaniss/anyswitch/discussions)

Converts Switch executables you already have in plain form into native AArch64 Linux programs, with a native runtime in place of the title's SDK. No emulator: the original machine code runs in-process after a handful of instructions are patched. The approach mirrors what [AnyPS5](https://github.com/boykopovar/AnyPS5) does for PS5 titles; [docs/FEASIBILITY.md](docs/FEASIBILITY.md) explains how it maps onto the Switch.

"anyswitch" is a working title.

## Status

Early Phase 1 of [docs/PLAN.md](docs/PLAN.md). No commercial title runs yet. Milestones live in [docs/roadmap.json](docs/roadmap.json); the badges, the map above and the [progress page](https://savvaniss.github.io/anyswitch/progress.html) are generated from the repository on every push.

| Piece | State |
|---|---|
| NSO / NRO / NPDM / MOD0 parsing, LZ4, hash checks | done |
| Relinker to AArch64 Linux ELF (PIE main, shared subsdk), symbol prefixing, relocation rebasing | done |
| Patching of `svc`, `tpidrro_el0`, `tpidr_el0`, counters, `cntfrq_el0`, exclusives | done |
| Traps that name missing SDK functions; `audit` and Phase 0 measurement tools | done |
| Runtime: start-up, module discovery, guest threads and TLS, SVC safety net, crash reports | done |
| SDK coverage: `nn::os` (time, threads, mutexes, args), C library basics, C++ ABI basics | started (94 exports) |
| `nn::fs`, `nn::hid`, audio, NVN graphics, shader recompiler | not started |
| macOS, Windows on ARM, 16 KiB pages, x86-64 hosts | not started |

Host tools build with GCC, Clang and MSVC. The runtime and the end-to-end corpus run on AArch64 Linux, or on x86-64 Linux/WSL under qemu-user.

## Quick start

```bash
tools/bootstrap/fetch_toolchain.sh   # Zig, qemu-user, AArch64 sysroot; no root needed
tools/dev/build.sh                   # build/host/anyswitch-relink, build/runtime/libnxrt.so
tools/dev/test.sh                    # unit tests and the synthetic corpus
```

Converting your own title, the input contract and exit statuses: [docs/user/USAGE.md](docs/user/USAGE.md).

## Contributing

Start with [AGENTS.md](AGENTS.md). It is the rulebook for human contributors and for AI coding agents alike: legal boundaries (no keys, no decryption, no game data, no SDK material, cite public sources), engineering rules, a repository map and the workflow. Then:

- [CONTRIBUTING.md](CONTRIBUTING.md): how to contribute and report results without sharing game data
- [docs/dev/ARCHITECTURE.md](docs/dev/ARCHITECTURE.md): how relinked modules and the runtime fit together
- [docs/dev/RECIPES.md](docs/dev/RECIPES.md): step-by-step for the common tasks
- [docs/dev/BUILD.md](docs/dev/BUILD.md), [docs/dev/TESTING.md](docs/dev/TESTING.md), [docs/dev/CONVENTIONS.md](docs/dev/CONVENTIONS.md)
- [docs/dev/debt/](docs/dev/debt/): known gaps per module, the best place to find work

## Background documents

- [docs/FEASIBILITY.md](docs/FEASIBILITY.md): what AnyPS5 is, how each of its enablers maps onto the Switch, prior art, risks
- [docs/PLAN.md](docs/PLAN.md): decisions, phases, sizing, open questions
- [docs/REUSE.md](docs/REUSE.md): every existing project that can help, with license and how we may use it

## Community

- [Discussions](https://github.com/savvaniss/anyswitch/discussions): one "Title bring-up" thread per title, without sharing any of its data
- [Issues](https://github.com/savvaniss/anyswitch/issues): bugs and missing SDK functions
- [SECURITY.md](SECURITY.md) and [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)

## License

MIT for the code in this repository. It grants nothing for any game, game data or firmware.
