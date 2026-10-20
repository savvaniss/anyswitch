## What and why

<!-- One module or concern per pull request. -->

## Sources

<!-- Public source for every layout, constant and behaviour (AGENTS.md L4):
     switchbrew pages, permissively licensed files, public documents, or
     "measured on hardware with homebrew: <how>". -->

## Checklist

- [ ] No keys, decryption, packaged formats or acquisition instructions (L1)
- [ ] No game data or files derived from a title (L2)
- [ ] No SDK material; every layout and constant cites a public source (L3, L4)
- [ ] No code copied from GPL or unlicensed projects; third-party code only as a submodule with a PROVENANCE entry (L5)
- [ ] No title-specific behaviour in `core/` or `runtime/` (L6)
- [ ] Unimplemented paths fail loudly; partial work is listed in `docs/dev/debt/<module>.md` (E1, E5)
- [ ] `nxabi.h` unchanged, or `NXABI_VERSION` bumped and ARCHITECTURE.md updated (E2)
- [ ] New guest exports are called through a real prototype from a corpus case (E4)

## Test results

<!-- Paste the summary lines. -->

```
tools/dev/test.sh:
tools/check_conventions.py:
```
