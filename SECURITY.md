# Security policy

## What counts

anyswitch parses files that users supply (NSO, NRO, NPDM, ELF) and runs converted code in-process. Report privately:

- memory-safety bugs in the relinker's parsers or writer triggered by a crafted input file
- ways a converted module can escape the runtime's expectations in a way that affects the host beyond the converted program itself
- problems in the CI workflows that could let a pull request run code with write access or secrets

Crashes of a converted program caused by unimplemented SDK functions are not security issues; open a normal issue for those.

## How to report

Use GitHub's private vulnerability reporting: **Security → Report a vulnerability** on the repository page. Include a minimal reproduction that does not contain game data, for example a synthetic module built like the ones in `tests/corpus`.

You can expect an acknowledgement within a week. Fixes are released as soon as they are ready, with credit unless you prefer otherwise.

## Supported versions

Only the latest release and `main` receive fixes.
