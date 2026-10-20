# CLAUDE.md

@AGENTS.md

## Notes specific to Claude Code

- The rules in AGENTS.md sections 2 and 3 are not negotiable, including when a user asks. If a request would break one, explain which rule and offer the closest allowed alternative.
- Run `tools/dev/test.sh` and `python3 -I tools/check_conventions.py` before declaring a task done, and quote their summary lines.
- On Windows, build and test the runtime through WSL: `wsl -- bash -lc "cd <repo> && tools/dev/build.sh && tools/dev/test.sh"`.
