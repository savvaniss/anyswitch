#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Mechanical checks for the rules in AGENTS.md. CI runs this on every pull
request; run it locally before opening one.

  python3 -I tools/check_conventions.py [--base origin/main]

--base enables checks that compare against a git revision (ABI version bump).
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SELF = Path(__file__).resolve()
SOURCE_DIRS = ["core", "runtime", "tests", "tools", "cmake"]
SOURCE_SUFFIXES = {".cpp", ".hpp", ".h", ".c", ".S", ".py", ".sh", ".cmake", ".ld", ".map"}
SKIP_DIRS = {".git", "__pycache__"}
# Game data and key material must never be committed (rules L1, L2).
FORBIDDEN_SUFFIXES = {".nso", ".nro", ".nrr", ".npdm", ".nsp", ".nsz", ".xci", ".xcz", ".nca", ".ncz", ".keys",
                      ".tik", ".bntx", ".bfres", ".bfsar", ".nacp", ".sarc", ".szs"}
MAX_FILE_BYTES = 2 * 1024 * 1024
# Terms that have no place in code, tests or tools (rule L1). Policy documents
# that name them in order to forbid them live outside SOURCE_DIRS.
FORBIDDEN_TERMS = [r"prod\.keys", r"title\.keys", r"\btitlekeys?\b", r"\bhactool", r"nxdumptool", r"lockpick",
                   r"\bdecrypt", r"keys\.txt"]
GPL_MARKERS = [r"GNU General Public License", r"SPDX-License-Identifier:\s*(L?GPL|AGPL)"]


def tracked_files() -> list[Path]:
    try:
        out = subprocess.run(["git", "ls-files", "-co", "--exclude-standard"], cwd=ROOT, capture_output=True,
                             text=True, check=True).stdout
        return [ROOT / line for line in out.splitlines() if line and (ROOT / line).is_file()]
    except (OSError, subprocess.CalledProcessError):
        files = []
        for p in ROOT.rglob("*"):
            rel = p.relative_to(ROOT).parts
            if p.is_file() and not (set(rel) & SKIP_DIRS) and not rel[0].startswith("build"):
                files.append(p)
        return files


def is_text(p: Path) -> bool:
    try:
        p.read_bytes()[:4096].decode("utf-8")
        return True
    except UnicodeDecodeError:
        return False


def in_source_dirs(p: Path) -> bool:
    rel = p.relative_to(ROOT).parts
    return bool(rel) and rel[0] in SOURCE_DIRS


def is_source(p: Path) -> bool:
    return in_source_dirs(p) and (p.suffix in SOURCE_SUFFIXES or p.name == "CMakeLists.txt"
                                  or p.name.startswith("zig-"))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--base", help="git revision to compare against (enables the ABI bump check)")
    args = ap.parse_args()
    problems: list[str] = []
    files = tracked_files()

    for p in files:
        rel = p.relative_to(ROOT).as_posix()
        if p.suffix.lower() in FORBIDDEN_SUFFIXES:
            problems.append(f"{rel}: file type is not allowed in the repository (AGENTS.md L2)")
        if p.stat().st_size > MAX_FILE_BYTES:
            problems.append(f"{rel}: larger than {MAX_FILE_BYTES // 1024} KiB; binaries do not belong here")
        if not is_text(p):
            continue
        text = p.read_text(encoding="utf-8")
        if "\r\n" in text:
            problems.append(f"{rel}: CRLF line endings (use LF)")
        if is_source(p) and "SPDX-License-Identifier: MIT" not in "\n".join(text.splitlines()[:5]):
            problems.append(f"{rel}: missing 'SPDX-License-Identifier: MIT' header")
        if in_source_dirs(p) and p.resolve() != SELF:
            for n, line in enumerate(text.splitlines(), 1):
                for term in FORBIDDEN_TERMS:
                    if re.search(term, line, re.IGNORECASE):
                        problems.append(f"{rel}:{n}: forbidden term /{term}/ (AGENTS.md L1)")
                for marker in GPL_MARKERS:
                    if re.search(marker, line):
                        problems.append(f"{rel}:{n}: GPL-licensed text in an MIT tree (AGENTS.md L5)")

    runtime_src = ROOT / "runtime" / "src"
    if runtime_src.is_dir():
        for module in sorted(d.name for d in runtime_src.iterdir() if d.is_dir()):
            if not (ROOT / "docs" / "dev" / "debt" / f"{module}.md").exists():
                problems.append(f"runtime/src/{module}: no docs/dev/debt/{module}.md (AGENTS.md E5)")
        seen: dict[str, str] = {}
        export = re.compile(r"NX_GUEST_(?:FN|VAR)\([^\"]*\"([^\"]+)\"")
        for p in sorted(runtime_src.rglob("*.cpp")):
            for n, line in enumerate(p.read_text(encoding="utf-8").splitlines(), 1):
                for sym in export.findall(line):
                    where = f"{p.relative_to(ROOT).as_posix()}:{n}"
                    if sym in seen:
                        problems.append(f"{where}: guest symbol {sym} already exported at {seen[sym]}")
                    seen[sym] = where

    if args.base:
        header = "core/abi/include/nxabi/nxabi.h"
        diff = subprocess.run(["git", "diff", args.base, "--", header], cwd=ROOT, capture_output=True, text=True)
        if diff.returncode == 0 and diff.stdout and not re.search(r"^\+#define NXABI_VERSION", diff.stdout, re.M):
            problems.append(f"{header} changed without bumping NXABI_VERSION (AGENTS.md E2)")

    for p in problems:
        print(p)
    print(f"check_conventions: {len(files)} files, {len(problems)} problem(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
