# Debt files

One file per module. Each bullet is a known gap: what is missing or simplified, what the console does (with a source, or "unknown"), and how anyswitch behaves today. Pull requests that leave something partial add a bullet (AGENTS.md rule E5); pull requests that close a gap remove it.

| File | Module |
|---|---|
| [relinker.md](relinker.md) | `core/` relinker and format library |
| [rt.md](rt.md) | `runtime/src/rt`: start-up, threads, SVC safety net, traps |
| [nn_os.md](nn_os.md) | `runtime/src/nn_os` |
| [libc.md](libc.md) | `runtime/src/libc` |
| [cxxabi.md](cxxabi.md) | `runtime/src/cxxabi` |

Template for a new module:

```markdown
# <module>

Source of truth for semantics: <links>.

## Gaps

- **<Function or area>.** Console: <behaviour, source>. anyswitch: <current behaviour>. Fails: <loudly / silently, how>.
```
