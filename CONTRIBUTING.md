# Contributing to anyswitch

Thank you for helping. This project lives or dies by two things: a clean legal footing and a runtime that fails loudly instead of guessing. Please read [AGENTS.md](AGENTS.md); its rules apply to human contributors exactly as they apply to AI agents.

## What you can work on

- **Runtime coverage.** Each `docs/dev/debt/<module>.md` lists known gaps. `tools/phase0/symbol_inventory.py` run on your own titles ranks missing functions by how many titles need them; share the aggregate ranking, never per-title lists.
- **Relinker.** New patch classes, NRO plugin loading, macOS and Windows-on-ARM output formats (PLAN.md Phase 5).
- **Corpus.** More synthetic cases that pin down behaviour, especially cases you have also run on real hardware as homebrew.
- **Tooling and docs.** Anything that makes the next contributor faster.

## Input contract

anyswitch reads only NSO and NRO files from an `exefs` folder the user already has in plain form. It never reads keys, never decrypts, never unpacks packaged content, and never explains how to get any of that. Issues and pull requests that ask for or add such functionality will be closed. See [docs/user/USAGE.md](docs/user/USAGE.md).

## Clean-room sourcing

- Semantics come from public sources: switchbrew, permissively licensed projects (libnx, deko3d, the MIT Ryujinx tree, envytools, Mesa, musl), Arm and NVIDIA public documents, and measurements of homebrew on hardware you own.
- Function names and prototypes may come from public symbol reconstructions (open-ead/nnheaders, nnsdk-rs). Structure layouts must be justified independently.
- GPL and unlicensed projects are reading material only. [docs/REUSE.md](docs/REUSE.md) lists the mode for each known project.
- Every layout, constant and behaviour carries a comment naming its source.

## Reporting results from your own titles

Bring-up reports help, but they must not leak game data. Post them in the "Title bring-up" category of [Discussions](https://github.com/savvaniss/anyswitch/discussions), one thread per title; the form asks only for safe fields. Share the first unimplemented function name, the `anyswitch-relink audit` summary line and the crash location as `module+offset`. Do not attach binaries, dumps, symbol lists, screenshots or logs that contain game text.

Missing SDK functions that block titles get an issue from the "Missing SDK function" template, so work can be coordinated.

## Automation you will meet

- **CI** builds and tests on Linux, Windows, macOS and native AArch64, and lints workflows and shell scripts.
- **Conventions** checks every pull request against AGENTS.md using the checker from `main`, so a pull request cannot change the rules it is judged by.
- **Progress comment** shows how your pull request changes the SDK export count, open gaps, corpus cases and roadmap.
- **Labels** are applied by area; `needs rebase` marks conflicts, and conflicted pull requests idle for 14 days get a reminder. Nothing is closed automatically.
- **Releases** are built and tested from `v*` tags.

## Pull requests

1. Fork, branch from `main`, keep the change to one module or concern.
2. Run `tools/dev/test.sh` and `python3 -I tools/check_conventions.py`.
3. Fill in the pull request template, including sources and debt-file changes.
4. Use Conventional Commits with the module as scope, for example `feat(nn_os): add event objects`.
5. By submitting, you agree your contribution is licensed under the MIT license and that you wrote it or have the right to submit it under that license.

## Code of conduct

Be kind and assume good faith. Review the code, not the person.
