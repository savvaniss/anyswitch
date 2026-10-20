### What
<!-- What changes and why. One topic per pull request. -->

### Tested
<!-- Host (CPU, OS, page size), how it ran (corpus case, homebrew, a title you own), what you checked. "Not tested" is a valid answer.
     Paste the summary lines of tools/dev/test.sh and tools/check_conventions.py.
     A speed claim shows before and after numbers from the same machine and the same corpus case or scene. -->

### Sources
<!-- The public source for every layout, constant, result code and behaviour (AGENTS.md L4), or "none needed". -->

### Checklist
<!-- Every line must be ticked; the rules behind them are in AGENTS.md and CONTRIBUTING.md. The Conventions check reads this section. -->
- [ ] Based on current `main`; no other open pull request implements the same functions (`tools/pr_overlap.py --branch <branch>`)
- [ ] One topic; follow-up work goes in a new pull request
- [ ] No keys, decryption, packaged formats or instructions for obtaining files (L1)
- [ ] No game data and nothing derived from a title, including logs and screenshots (L2)
- [ ] No SDK material; every layout, constant and behaviour cites a public source (L3, L4)
- [ ] No code copied from GPL or unlicensed projects; new third-party code is a submodule under `3rdparty/` (L5)
- [ ] General behaviour only; nothing specific to one title (L6)
- [ ] Unimplemented paths fail loudly; partial work and silent stubs are listed in `docs/dev/debt/<module>.md` (E1, E5)
- [ ] `nxabi.h` unchanged, or `NXABI_VERSION` bumped and ARCHITECTURE.md updated (E2)
- [ ] New exports are called through a real prototype from the corpus (E4)
- [ ] No notes, logs, images or investigation files added to the repository
- [ ] Depends on: <!-- #N, or none -->
- [ ] Fixes: <!-- #N (closes the issue on merge), or none -->
- [ ] Unblocks: <!-- title name and its Title bring-up discussion, for example "Example Game #12", or none -->
- [ ] GPUs tested: <!-- graphics changes: vendor, model, driver and OS of each GPU; otherwise n/a -->
- [ ] AI-assisted: <!-- yes / no -->
