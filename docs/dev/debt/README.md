# Debt files

One file per module. Each entry is a known gap: what differs from the console, what is unverified, and how anyswitch fails today. Pull requests that leave something partial add an entry (AGENTS.md E5); pull requests that close a gap remove the entry, or cut it to what is still open, instead of rewriting it to describe the new behaviour.

| File | Module |
|---|---|
| [relinker.md](relinker.md) | `core/` relinker and format library |
| [rt.md](rt.md) | `runtime/src/rt`: start-up, threads, SVC safety net, traps |
| [nn_os.md](nn_os.md) | `runtime/src/nn_os` |
| [libc.md](libc.md) | `runtime/src/libc` |
| [cxxabi.md](cxxabi.md) | `runtime/src/cxxabi` |

## Entry format

- One bullet per gap, starting with the area in bold. State the gap first, then only as much of the current behaviour as the gap needs.
- Stay under 1000 characters (rule `debt-length` warns).
- End with `Closes when:` and the measurement, test or title that would close it, when one exists, so re-checkable entries can be found by searching for it.
- Assumptions that a corpus case encodes say so: "Assumption, encoded in `tests/corpus/src/<case>.cpp`".

## Silent stubs

A function that returns a constant without doing its work is allowed only when it unblocks a title and only affects presentation (AGENTS.md E1). List it under a `## Silent stubs` heading in its module's file, by C name or mangled symbol, with what the console would do. The `silent-stub` rule fails for any constant-returning export not listed there.

## Template for a new module

```markdown
# <module>

Source of truth for semantics: <links>.

## Gaps

- **<Function or area>.** Console: <behaviour, source>. anyswitch: <current behaviour>. Fails: <loudly / silently, how>. Closes when: <measurement or test>.

## Silent stubs

(none)
```
