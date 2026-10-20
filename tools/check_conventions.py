#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Mechanical checks for the rules in AGENTS.md. CI runs this on every push
and pull request; run it locally before opening one.

  python3 -I tools/check_conventions.py                     check the working tree
  python3 -I tools/check_conventions.py --base REV          also check the ABI version bump against REV
  python3 -I tools/check_conventions.py --base B --head H   check commit H as git objects (used by the
                                                            pull request workflow, which never checks out
                                                            or runs pull request code)
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parents[1]
SELF = "tools/check_conventions.py"
SOURCE_DIRS = {"core", "runtime", "tests", "tools", "cmake", "site"}
SOURCE_SUFFIXES = {".cpp", ".hpp", ".h", ".c", ".S", ".py", ".sh", ".cmake", ".ld", ".map", ".css"}
SKIP_DIRS = {".git", "__pycache__"}
# Game data and key material must never be committed (rules L1, L2).
FORBIDDEN_SUFFIXES = {".nso", ".nro", ".nrr", ".npdm", ".nsp", ".nsz", ".xci", ".xcz", ".nca", ".ncz", ".keys",
                      ".tik", ".bntx", ".bfres", ".bfsar", ".nacp", ".sarc", ".szs"}
MAX_FILE_BYTES = 2 * 1024 * 1024
# Terms that have no place in code, tests, tools or the site (rule L1). Policy
# documents that name them in order to forbid them live outside SOURCE_DIRS.
FORBIDDEN_TERMS = [r"prod\.keys", r"title\.keys", r"\btitlekeys?\b", r"\bhactool", r"nxdumptool", r"lockpick",
                   r"\bdecrypt", r"keys\.txt"]
GPL_MARKERS = [r"GNU General Public License", r"SPDX-License-Identifier:\s*(L?GPL|AGPL)"]
ROADMAP_STATUSES = {"done", "in_progress", "planned", "blocked"}


def git(*args: str) -> subprocess.CompletedProcess:
    return subprocess.run(["git", *args], cwd=ROOT, capture_output=True)


class WorkTree:
    """Files of the working tree that git would track."""

    def __init__(self) -> None:
        r = git("ls-files", "-co", "--exclude-standard", "-z")
        if r.returncode == 0:
            self.paths = sorted(p for p in r.stdout.decode().split("\0") if p and (ROOT / p).is_file())
        else:
            self.paths = sorted(p.relative_to(ROOT).as_posix() for p in ROOT.rglob("*")
                                if p.is_file() and not (set(p.relative_to(ROOT).parts) & SKIP_DIRS)
                                and not p.relative_to(ROOT).parts[0].startswith("build"))

    def read(self, path: str) -> bytes:
        return (ROOT / path).read_bytes()


class GitTree:
    """Files of a commit, read as git objects without checking them out."""

    def __init__(self, rev: str) -> None:
        self.rev = rev
        r = git("ls-tree", "-r", "-z", "--name-only", rev)
        if r.returncode != 0:
            sys.exit(f"cannot list {rev}: {r.stderr.decode().strip()}")
        self.paths = sorted(p for p in r.stdout.decode().split("\0") if p)

    def read(self, path: str) -> bytes:
        r = git("cat-file", "blob", f"{self.rev}:{path}")
        if r.returncode != 0:
            sys.exit(f"cannot read {self.rev}:{path}")
        return r.stdout


def decode(data: bytes) -> str | None:
    try:
        return data.decode("utf-8")
    except UnicodeDecodeError:
        return None


def is_source(path: str) -> bool:
    p = PurePosixPath(path)
    return p.parts[0] in SOURCE_DIRS and (p.suffix in SOURCE_SUFFIXES or p.name == "CMakeLists.txt"
                                          or p.name.startswith("zig-"))


def check_roadmap(text: str, problems: list[str]) -> None:
    try:
        data = json.loads(text)
        seen = set()
        for phase in data["phases"]:
            for m in phase["milestones"]:
                if m["status"] not in ROADMAP_STATUSES:
                    problems.append(f"docs/roadmap.json: {m['id']}: unknown status '{m['status']}'")
                if m["id"] in seen:
                    problems.append(f"docs/roadmap.json: duplicate milestone id {m['id']}")
                seen.add(m["id"])
    except (ValueError, KeyError, TypeError) as e:
        problems.append(f"docs/roadmap.json: invalid ({e})")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--base", help="git revision to compare against (enables the ABI bump check)")
    ap.add_argument("--head", help="check this commit's objects instead of the working tree")
    args = ap.parse_args()
    tree = GitTree(args.head) if args.head else WorkTree()
    problems: list[str] = []
    exports: dict[str, str] = {}
    runtime_modules: set[str] = set()
    debt_files: set[str] = set()
    export = re.compile(r"NX_GUEST_(?:FN|VAR)\([^\"]*\"([^\"]+)\"")

    for path in tree.paths:
        parts = PurePosixPath(path).parts
        if PurePosixPath(path).suffix.lower() in FORBIDDEN_SUFFIXES:
            problems.append(f"{path}: file type is not allowed in the repository (AGENTS.md L2)")
        data = tree.read(path)
        if len(data) > MAX_FILE_BYTES:
            problems.append(f"{path}: larger than {MAX_FILE_BYTES // 1024} KiB; binaries do not belong here")
        if len(parts) >= 4 and parts[:2] == ("runtime", "src"):
            runtime_modules.add(parts[2])
        if len(parts) == 4 and parts[:3] == ("docs", "dev", "debt"):
            debt_files.add(PurePosixPath(path).stem)
        text = decode(data)
        if text is None:
            continue
        if "\r\n" in text:
            problems.append(f"{path}: CRLF line endings (use LF)")
        if is_source(path) and "SPDX-License-Identifier: MIT" not in "\n".join(text.splitlines()[:5]):
            problems.append(f"{path}: missing 'SPDX-License-Identifier: MIT' header")
        if parts[0] in SOURCE_DIRS and path != SELF:
            for n, line in enumerate(text.splitlines(), 1):
                for term in FORBIDDEN_TERMS:
                    if re.search(term, line, re.IGNORECASE):
                        problems.append(f"{path}:{n}: forbidden term /{term}/ (AGENTS.md L1)")
                for marker in GPL_MARKERS:
                    if re.search(marker, line):
                        problems.append(f"{path}:{n}: GPL-licensed text in an MIT tree (AGENTS.md L5)")
        if parts[:2] == ("runtime", "src") and path.endswith(".cpp"):
            for n, line in enumerate(text.splitlines(), 1):
                for sym in export.findall(line):
                    if sym in exports:
                        problems.append(f"{path}:{n}: guest symbol {sym} already exported at {exports[sym]}")
                    exports[sym] = f"{path}:{n}"
        if path == "docs/roadmap.json":
            check_roadmap(text, problems)

    for module in sorted(runtime_modules - debt_files):
        problems.append(f"runtime/src/{module}: no docs/dev/debt/{module}.md (AGENTS.md E5)")

    if args.base:
        header = "core/abi/include/nxabi/nxabi.h"
        rev_args = [args.base, args.head] if args.head else [args.base]
        diff = git("diff", *rev_args, "--", header)
        text = diff.stdout.decode(errors="replace")
        if diff.returncode == 0 and text and not re.search(r"^\+#define NXABI_VERSION", text, re.M):
            problems.append(f"{header} changed without bumping NXABI_VERSION (AGENTS.md E2)")

    for p in problems:
        print(p)
    print(f"check_conventions: {len(tree.paths)} files, {len(problems)} problem(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
