# Contributing to anyswitch

Thank you for helping. This project depends on two things: a clean legal footing and a runtime that fails loudly instead of guessing. The rules are in [AGENTS.md](AGENTS.md) and apply to human contributors exactly as they apply to AI agents; this page explains the process around them.

## What you can work on

- **Runtime coverage.** Each `docs/dev/debt/<module>.md` lists known gaps. `tools/phase0/symbol_inventory.py` run on your own titles ranks missing functions by how many titles need them; share the aggregate ranking, never per-title lists. Missing functions that block titles get an issue from the "Missing SDK function" template.
- **Relinker.** New patch classes, NRO plugin loading, macOS and Windows-on-ARM output formats (PLAN.md Phase 5).
- **Corpus.** More synthetic cases that pin down behaviour, especially cases you have also run on real hardware as homebrew.
- **Tooling and docs.** Anything that makes the next contributor faster.

## Input contract

anyswitch reads only NSO and NRO files from an `exefs` folder the user already has in plain form. It never reads keys, never handles encrypted content, never unpacks packaged content, and never explains how to get any of that. Issues and pull requests that ask for or add such functionality are closed. See [docs/user/USAGE.md](docs/user/USAGE.md).

## Code

- Follow [AGENTS.md](AGENTS.md) sections 2 and 3 and the [conventions](docs/dev/CONVENTIONS.md).
- Every function either does exactly what it should for every input or stops with a message naming itself. A silent stub is allowed only when it unblocks a title and only affects presentation; it is listed under "Silent stubs" in its module's debt file.
- Missing imports fail with a clear error (a named trap, or the loader refusing to start); never replace them with a fallback that keeps running.
- Only the SDK and code that reaches the kernel or GPU are reimplemented. Middleware a title ships in `subsdkN` is relinked and loaded as it is.
- Semantics come from public sources: switchbrew, permissively licensed projects (libnx, deko3d, the MIT Ryujinx tree, envytools, Mesa, musl), Arm and NVIDIA public documents, and measurements of homebrew on hardware you own. Pull requests that add or change shader instruction semantics name the exact source or the hardware measurement; reviewers may ask for a measurement.
- Function names and prototypes may come from public symbol reconstructions (open-ead/nnheaders, nnsdk-rs). Structure layouts must be justified independently.
- GPL and unlicensed projects, AnyPS5 included, are reading material only. [docs/REUSE.md](docs/REUSE.md) lists the mode for each known project.
- Third-party code is a submodule under `3rdparty/`, built from source with CMake, with an entry in [3rdparty/PROVENANCE.md](3rdparty/PROVENANCE.md). Nothing is found on the system.
- Each corpus case finishes within 30 s on CI runners, qemu-user included. CI warns about slower cases; split or shrink them.

## Pull requests

The [pull request template](.github/PULL_REQUEST_TEMPLATE.md) is the checklist for these rules, and the Conventions check reads it.

- One branch per topic, based on current `main`. Follow-up work goes in a new pull request, not into an open one.
- Before starting, check that no open pull request already implements the same functions. Before opening, run `python3 -I tools/pr_overlap.py --branch <branch>`. A bot also comments on open pull requests that change the same lines or add the same exports; agree with the other author which goes first and record it with `Depends on: #N`.
- Keep the branch up to date with `main` and resolve conflicts yourself; rebasing and force-pushing are fine. A conflicting pull request is labeled `needs rebase`, and after 14 days without activity it gets a reminder and the `stale` label. Nothing is closed for being stale.
- If the pull request needs another one first, say so on the `Depends on` line (`none` otherwise).
- If it fixes a reported issue, write `Fixes: #N` on its line (`none` otherwise). GitHub closes the issue on merge, and the pull request gets the `fixes issue` label.
- If it fixes a stop or a wrong result seen on a title, name the title and its "Title bring-up" discussion on the `Unblocks` line (`none` otherwise). These pull requests get the `unblocks title` label and are reviewed first. A bring-up thread lists the open pull requests a title still needs; update it when one is merged, closed or replaced.
- Run `tools/dev/test.sh` and `python3 -I tools/check_conventions.py --base origin/main` before opening, and describe what was tested (host, corpus case, homebrew or title).
- A pull request that claims a change in speed shows before and after numbers from the same machine and the same corpus case or scene. Titles starting with `perf` get the `perf` label.
- Investigation notes, reports, screenshots and logs go in the pull request, not in the repository.
- Say whether the change was written with AI assistance (`AI-assisted: yes/no`), and keep AI-generated analysis apart from measured results. The author of the pull request is responsible for every line of it.
- Open pull requests per author are limited by how many of theirs are merged into `main`: 1 with none merged, 20 with at least one, no limit from 20. A pull request opened over the limit is closed automatically with the `open PR limit` label; reopen it once another is merged or closed. Collaborators with write access are exempt.

## Continuous integration

- **CI** (`ci.yml`) builds and tests the host tools on Linux, Windows and macOS, builds the runtime and runs every corpus case under qemu-user and natively on AArch64, lints workflows and shell scripts, and builds the Pages site. It runs on every push to `main` and on pull requests that change more than Markdown.
- **Conventions** (`conventions.yml`) runs for every pull request, first-time contributors included. It uses the checker from the base branch and reads the pull request's commits as git objects without executing them, then checks the title, commit subjects and template. Run it locally with `python3 -I tools/check_conventions.py --base origin/main`; `--list-rules` explains every rule.
- Every workflow job that runs for `pull_request` filters on `paths` or `paths-ignore`; the Conventions check fails a workflow change that adds one without a filter.
- **Progress comment** shows how a pull request changes the SDK export count, open gaps, corpus cases and roadmap. **PR overlap** comments on overlapping pull requests. **PR labels** applies area labels and the labels described above.
- GitHub asks a maintainer to approve workflow runs for first-time contributors; this is expected.
- **Releases** are built and tested from `v*` tags; see [docs/dev/RECIPES.md](docs/dev/RECIPES.md#cut-a-release).

## Reporting results from your own titles

Bring-up reports help, but they must not leak game data. Post them in the "Title bring-up" category of [Discussions](https://github.com/savvaniss/anyswitch/discussions), one thread per title; the form asks only for safe fields. [docs/dev/TITLE_TESTING.md](docs/dev/TITLE_TESTING.md) describes how to run a title and what to report.

## Documentation

Text in the repository is dry, strict and concise. Known gaps go in `docs/dev/debt/<module>.md`, not in new files: an entry states the gap first (what differs from the console, what is unverified, what fails) and ends with `Closes when:` and the measurement or test that would close it, when there is one. A pull request that closes a gap removes its entry, or cuts it to what is still open, instead of rewriting it to describe the new behaviour. Images for documentation go in the pull request or issue, not in the repository.

## License of contributions

By submitting a pull request you agree that your contribution is licensed under the MIT license and that you wrote it or have the right to submit it under that license.

## Code of conduct

See [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).
