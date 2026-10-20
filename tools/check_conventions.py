#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Mechanical checks for the rules in AGENTS.md and CONTRIBUTING.md. CI runs
this on every push and pull request; run it locally before pushing.

  python3 -I tools/check_conventions.py                    the working tree
  python3 -I tools/check_conventions.py --base origin/main also commit subjects and the ABI bump since BASE
  python3 -I tools/check_conventions.py --base B --head H  commit H read as git objects; the pull request
                                                           workflow uses this and never runs PR code
  ... --pr                                                 also the pull request title and body, read
                                                           from PR_TITLE, PR_BODY and PR_AUTHOR

Every finding names its rule; --list-rules prints them with the document
that explains each one.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parents[1]
SELF = "tools/check_conventions.py"

RULES = {
    "spdx": ("error", "docs/dev/CONVENTIONS.md#files", "Source files start with 'SPDX-License-Identifier: MIT'"),
    "line-endings": ("error", "docs/dev/CONVENTIONS.md#files", "Text files use LF line endings"),
    "game-data": ("error", "AGENTS.md#2-hard-rules-legal-and-provenance-never-break-these",
                  "Game data and key material never enter the repository (L1, L2)"),
    "large-file": ("error", "docs/dev/CONVENTIONS.md#files", "Files over 2 MiB do not belong in the repository"),
    "binary": ("error", "docs/dev/CONVENTIONS.md#files",
               "Only UTF-8 text files; images and logs go in the pull request or issue"),
    "forbidden-term": ("error", "AGENTS.md#2-hard-rules-legal-and-provenance-never-break-these",
                       "Code, tests, tools and the site never mention keys, decryption or dumping tools (L1)"),
    "gpl-text": ("error", "AGENTS.md#2-hard-rules-legal-and-provenance-never-break-these",
                 "No GPL-licensed text in this MIT tree (L5)"),
    "system-dependency": ("error", "AGENTS.md#2-hard-rules-legal-and-provenance-never-break-these",
                          "Third-party code is a submodule under 3rdparty/ built from source, never found on the system (L5)"),
    "title-specific": ("error", "AGENTS.md#2-hard-rules-legal-and-provenance-never-break-these",
                       "No program IDs or other title-specific values in core/ or runtime/ (L6)"),
    "silent-stub": ("error", "AGENTS.md#3-hard-rules-engineering",
                    "An export that only returns a constant calls NX_UNIMPLEMENTED, or is listed under 'Silent stubs' in its debt file (E1)"),
    "debt-file": ("error", "AGENTS.md#3-hard-rules-engineering", "Every runtime module has docs/dev/debt/<module>.md (E5)"),
    "duplicate-export": ("error", "docs/dev/CONVENTIONS.md#guest-exports", "A guest symbol is exported once"),
    "abi-version": ("error", "AGENTS.md#3-hard-rules-engineering",
                    "A change to nxabi.h bumps NXABI_VERSION (E2)"),
    "extension": ("error", "AGENTS.md#3-hard-rules-engineering",
                  "core/ uses standard C++ only: no __attribute__, __declspec or #pragma other than once (E6)"),
    "env-reference": ("error", "docs/dev/ENVIRONMENT.md",
                      "Every NXRT_* and ANYSWITCH_* variable or option the code reads has a row in docs/dev/ENVIRONMENT.md, and every row is still read"),
    "notes-file": ("error", "AGENTS.md#4-pull-request-rules",
                   "Notes, logs, patches and agent configuration go in the pull request, not the repository"),
    "doc-link": ("error", "docs/dev/CONVENTIONS.md#documentation", "Relative link to a file that does not exist"),
    "ci-gate": ("error", "CONTRIBUTING.md#continuous-integration",
                "A workflow triggered by pull_request filters on paths or paths-ignore"),
    "roadmap": ("error", "docs/dev/RECIPES.md#complete-a-roadmap-milestone", "docs/roadmap.json is valid"),
    "debt-length": ("warning", "docs/dev/debt/README.md",
                    "Debt entries stay under 1000 characters: state the gap first, keep the rest short"),
    "commit-subject": ("error", "docs/dev/CONVENTIONS.md#commits", "Commit subjects follow Conventional Commits"),
    "pr-title": ("error", "docs/dev/CONVENTIONS.md#commits", "Pull request titles follow Conventional Commits"),
    "pr-template": ("error", ".github/PULL_REQUEST_TEMPLATE.md", "Fill in every part of the pull request template"),
}

SOURCE_DIRS = {"core", "runtime", "tests", "tools", "cmake", "site"}
SOURCE_SUFFIXES = {".cpp", ".hpp", ".h", ".c", ".S", ".py", ".sh", ".cmake", ".ld", ".map", ".css"}
SKIP_DIRS = {".git", "__pycache__"}
FORBIDDEN_SUFFIXES = {".nso", ".nro", ".nrr", ".npdm", ".nsp", ".nsz", ".xci", ".xcz", ".nca", ".ncz", ".keys",
                      ".tik", ".bntx", ".bfres", ".bfsar", ".nacp", ".sarc", ".szs"}
MAX_FILE_BYTES = 2 * 1024 * 1024
FORBIDDEN_TERMS = [r"prod\.keys", r"title\.keys", r"\btitlekeys?\b", r"\bhactool", r"nxdumptool", r"lockpick",
                   r"\bdecrypt", r"keys\.txt"]
GPL_MARKERS = [r"GNU General Public License", r"SPDX-License-Identifier:\s*(L?GPL|AGPL)"]
PROGRAM_ID = re.compile(r"(?<![0-9a-fA-F])0100[0-9a-fA-F]{12}(?![0-9a-fA-F])")
EXTENSION = re.compile(r"__attribute__|__declspec|#\s*pragma\s+(?!once\b)")
SYSTEM_DEPENDENCY = re.compile(r"\b(find_package\s*\((?!\s*(Python3|Threads|Git)\b)|pkg_check_modules\b|"
                               r"pkg_search_module\b|find_(library|path|file)\s*\()", re.I)
NOTE_SUFFIXES = {".md", ".markdown", ".rst", ".log", ".patch", ".diff"}
ALLOWED_MARKDOWN = [r"[A-Z_]+\.md", r"docs/[^/]+\.md", r"docs/dev/[^/]+\.md", r"docs/dev/debt/[^/]+\.md",
                    r"docs/user/[^/]+\.md", r"3rdparty/PROVENANCE\.md", r"\.github/.+\.md"]
AGENT_FILES = {"agents.md", "claude.md", "gemini.md", ".cursorrules", "copilot-instructions.md", ".windsurfrules",
               "chatgpt.md", ".aider.conf.yml", ".clinerules"}
ALLOWED_AGENT_FILES = {"AGENTS.md", "CLAUDE.md", ".github/copilot-instructions.md"}
AGENT_DIRS = {".claude", ".cursor", ".windsurf", ".aider", ".continue"}
CONVENTIONAL = re.compile(r"^(feat|fix|docs|style|refactor|perf|test|build|ci|chore|revert)(\([^)]+\))?!?: \S")
LINK = re.compile(r"(?<!!)\[[^\]]*\]\(([^)\s]+)\)|!\[[^\]]*\]\(([^)\s]+)\)")
FENCE = re.compile(r"^ {0,3}(```|~~~).*?^ {0,3}\1[ \t]*$", re.S | re.M)
INLINE_CODE = re.compile(r"`[^`\n]*`")
GUEST_EXPORT = re.compile(r"NX_GUEST_(FN|VAR)\(\s*[^,]+,\s*(\w+)\s*,\s*\"([^\"]+)\"")
STUB_BODY = re.compile(r"^(?:\(void\)\s*\w+\s*;)*(?:return\s*(?:-?(?:0x[0-9a-fA-F]+|\d+)[uUlL]*|nullptr|true|false|"
                       r"(?:\w+::)*kSuccess)?\s*;)?$")
_NAME = r"((?:NXRT|ANYSWITCH)_[A-Z0-9_]+)"
ENV_READ = {
    "cpp": [re.compile(r"getenv\(\s*\"" + _NAME + r"\"")],
    "python": [re.compile(r"os\.environ(?:\.get)?\s*[\[(]\s*[\"']" + _NAME + r"[\"']")],
    "shell": [re.compile(r"\$\{?" + _NAME)],
    "cmake": [re.compile(r"\$ENV\{" + _NAME + r"\}"), re.compile(r"\boption\(\s*" + _NAME)],
}
ENV_ROW = re.compile(r"^\|\s*`((?:NXRT|ANYSWITCH)_[A-Z0-9_]+)`", re.M)
ROADMAP_STATUSES = {"done", "in_progress", "planned", "blocked"}


def git(*args: str) -> subprocess.CompletedProcess:
    return subprocess.run(["git", *args], cwd=ROOT, capture_output=True)


class WorkTree:
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


class Checker:
    def __init__(self, tree) -> None:
        self.tree = tree
        self.paths = set(tree.paths)
        self.dirs = {str(PurePosixPath(p).parent) for p in tree.paths}
        for p in list(self.dirs):
            while p not in ("", "."):
                p = str(PurePosixPath(p).parent)
                self.dirs.add(p)
        self.findings: list[tuple[str, str, int, str]] = []
        self.texts: dict[str, str] = {}

    def report(self, rule: str, path: str, line: int = 0, detail: str = "") -> None:
        self.findings.append((rule, path, line, detail))

    def text(self, path: str) -> str | None:
        if path not in self.texts:
            try:
                self.texts[path] = self.tree.read(path).decode("utf-8")
            except UnicodeDecodeError:
                self.texts[path] = None
        return self.texts[path]

    # ---- per-file rules ------------------------------------------------------
    def check_files(self) -> None:
        for path in self.tree.paths:
            p = PurePosixPath(path)
            top = p.parts[0]
            if p.suffix.lower() in FORBIDDEN_SUFFIXES:
                self.report("game-data", path)
            data = self.tree.read(path)
            if len(data) > MAX_FILE_BYTES:
                self.report("large-file", path, 0, f"{len(data)} bytes")
            self.check_notes(path)
            text = self.text(path)
            if text is None:
                self.report("binary", path)
                continue
            if "\r\n" in text:
                self.report("line-endings", path)
            source = top in SOURCE_DIRS and (p.suffix in SOURCE_SUFFIXES or p.name == "CMakeLists.txt"
                                             or p.name.startswith("zig-"))
            if source and "SPDX-License-Identifier: MIT" not in "\n".join(text.splitlines()[:5]):
                self.report("spdx", path)
            lines = text.splitlines()
            if top in SOURCE_DIRS and path != SELF:
                for n, line in enumerate(lines, 1):
                    for term in FORBIDDEN_TERMS:
                        if re.search(term, line, re.IGNORECASE):
                            self.report("forbidden-term", path, n, term)
                    for marker in GPL_MARKERS:
                        if re.search(marker, line):
                            self.report("gpl-text", path, n)
            if top in ("core", "runtime"):
                for n, line in enumerate(lines, 1):
                    if PROGRAM_ID.search(line):
                        self.report("title-specific", path, n, PROGRAM_ID.search(line).group(0))
            if top == "core" and p.suffix in {".cpp", ".hpp", ".h", ".c"}:
                for n, line in enumerate(lines, 1):
                    if EXTENSION.search(line):
                        self.report("extension", path, n, EXTENSION.search(line).group(0))
            if p.name == "CMakeLists.txt" or p.suffix == ".cmake":
                for n, line in enumerate(lines, 1):
                    code = line.split("#", 1)[0]
                    if SYSTEM_DEPENDENCY.search(code):
                        self.report("system-dependency", path, n, code.strip())
            if p.suffix == ".md":
                self.check_links(path, text)
            if path.startswith(".github/workflows/") and p.suffix in {".yml", ".yaml"}:
                self.check_ci_gate(path, lines)
            if path == "docs/roadmap.json":
                self.check_roadmap(text)
            if path.startswith("docs/dev/debt/") and p.name != "README.md":
                for n, line in enumerate(lines, 1):
                    if line.startswith("- ") and len(line) > 1000:
                        self.report("debt-length", path, n, f"{len(line)} characters")

    def check_notes(self, path: str) -> None:
        p = PurePosixPath(path)
        if p.parts[0] in AGENT_DIRS:
            self.report("notes-file", path, 0, "agent configuration directory")
            return
        if p.name.lower() in AGENT_FILES and path not in ALLOWED_AGENT_FILES:
            self.report("notes-file", path, 0, "agent instruction file outside the allowed ones")
            return
        if p.suffix.lower() in NOTE_SUFFIXES and not any(re.fullmatch(rx, path) for rx in ALLOWED_MARKDOWN):
            self.report("notes-file", path)

    def check_links(self, path: str, text: str) -> None:
        body = FENCE.sub(lambda m: "\n" * m.group(0).count("\n"), text)
        base = PurePosixPath(path).parent
        for n, line in enumerate(body.splitlines(), 1):
            line = INLINE_CODE.sub("", line)
            for m in LINK.finditer(line):
                target = m.group(1) or m.group(2)
                if re.match(r"^[a-z][a-z0-9+.-]*:", target, re.I) or target.startswith("#"):
                    continue
                target = target.split("#", 1)[0].split("?", 1)[0]
                if not target:
                    continue
                resolved = os.path.normpath(str(base / target)).replace("\\", "/")
                if resolved.startswith(".."):
                    continue
                if resolved not in self.paths and resolved.rstrip("/") not in self.dirs:
                    self.report("doc-link", path, n, target)

    def check_ci_gate(self, path: str, lines: list[str]) -> None:
        in_on = False
        on_lines: list[str] = []
        for line in lines:
            if re.match(r"^on:\s*(\S.*)?$", line):
                in_on = True
                inline = line.split(":", 1)[1].strip()
                if re.search(r"\bpull_request\b(?!_)", inline):
                    self.report("ci-gate", path, 0, "inline trigger list cannot carry a filter")
                continue
            if in_on:
                if line and not line[0].isspace():
                    break
                on_lines.append(line)
        for i, line in enumerate(on_lines):
            m = re.match(r"^(\s+)pull_request:\s*$", line)
            if not m:
                continue
            indent = len(m.group(1))
            block = []
            for nxt in on_lines[i + 1:]:
                if nxt.strip() and len(nxt) - len(nxt.lstrip()) <= indent:
                    break
                block.append(nxt)
            if not any(re.match(r"^\s+(paths|paths-ignore):", b) for b in block):
                self.report("ci-gate", path, 0, "pull_request trigger without paths or paths-ignore")

    def check_roadmap(self, text: str) -> None:
        try:
            data = json.loads(text)
            seen = set()
            for phase in data["phases"]:
                for m in phase["milestones"]:
                    if m["status"] not in ROADMAP_STATUSES:
                        self.report("roadmap", "docs/roadmap.json", 0, f"{m['id']}: unknown status {m['status']}")
                    if m["id"] in seen:
                        self.report("roadmap", "docs/roadmap.json", 0, f"duplicate id {m['id']}")
                    seen.add(m["id"])
        except (ValueError, KeyError, TypeError) as e:
            self.report("roadmap", "docs/roadmap.json", 0, str(e))

    # ---- cross-file rules ----------------------------------------------------
    def check_runtime(self) -> None:
        modules = {PurePosixPath(p).parts[2] for p in self.tree.paths
                   if p.startswith("runtime/src/") and len(PurePosixPath(p).parts) >= 4}
        debt = {PurePosixPath(p).stem for p in self.tree.paths
                if p.startswith("docs/dev/debt/") and p.endswith(".md")}
        for module in sorted(modules - debt):
            self.report("debt-file", f"runtime/src/{module}")
        seen: dict[str, str] = {}
        for path in self.tree.paths:
            if not (path.startswith("runtime/src/") and path.endswith(".cpp")):
                continue
            text = self.text(path) or ""
            module = PurePosixPath(path).parts[2]
            stubs_doc = self.silent_stubs(f"docs/dev/debt/{module}.md")
            for m in GUEST_EXPORT.finditer(text):
                kind, cname, symbol = m.groups()
                line = text.count("\n", 0, m.start()) + 1
                if symbol in seen:
                    self.report("duplicate-export", path, line, f"{symbol} also at {seen[symbol]}")
                seen[symbol] = f"{path}:{line}"
                if kind == "FN":
                    body = self.function_body(text, m.end())
                    if body is not None and STUB_BODY.match(re.sub(r"\s+", "", body)) and \
                            symbol not in stubs_doc and cname not in stubs_doc:
                        self.report("silent-stub", path, line, symbol)

    def silent_stubs(self, path: str) -> str:
        text = self.text(path) if path in self.paths else None
        if not text:
            return ""
        m = re.search(r"^##+\s*Silent stubs\s*$(.*?)(?=^##+\s|\Z)", text, re.M | re.S)
        return m.group(1) if m else ""

    @staticmethod
    def function_body(text: str, start: int) -> str | None:
        depth = 0
        i = start
        # skip the rest of the macro's parameter list
        while i < len(text) and text[i] != "{":
            if text[i] == ";":
                return None
            i += 1
        begin = i
        while i < len(text):
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
                if depth == 0:
                    body = text[begin + 1:i]
                    body = re.sub(r"//[^\n]*", "", body)
                    return re.sub(r"/\*.*?\*/", "", body, flags=re.S)
            elif text[i] == '"':
                i += 1
                while i < len(text) and text[i] != '"':
                    i += 2 if text[i] == "\\" else 1
            i += 1
        return None

    def check_environment(self) -> None:
        doc = "docs/dev/ENVIRONMENT.md"
        read: dict[str, str] = {}
        for path in self.tree.paths:
            p = PurePosixPath(path)
            if p.parts[0] not in {"core", "runtime", "tools", "cmake"} and path != "CMakeLists.txt":
                continue
            if path == SELF:
                continue
            if p.suffix in {".cpp", ".hpp", ".h", ".c"}:
                language = "cpp"
            elif p.suffix == ".py":
                language = "python"
            elif p.suffix == ".sh" or p.name.startswith("zig-"):
                language = "shell"
            elif p.name == "CMakeLists.txt" or p.suffix == ".cmake":
                language = "cmake"
            else:
                continue
            text = self.text(path)
            if not text:
                continue
            for n, line in enumerate(text.splitlines(), 1):
                for rx in ENV_READ[language]:
                    for name in rx.findall(line):
                        read.setdefault(name, f"{path}:{n}")
        rows = set(ENV_ROW.findall(self.text(doc) or "")) if doc in self.paths else set()
        for name, where in sorted(read.items()):
            if name not in rows:
                path, line = where.rsplit(":", 1)
                self.report("env-reference", path, int(line), f"{name} has no row in {doc}")
        for name in sorted(rows - set(read)):
            self.report("env-reference", doc, 0, f"{name} has a row but nothing reads it")

    def check_submodules(self) -> None:
        text = self.text(".gitmodules") if ".gitmodules" in self.paths else None
        if not text:
            return
        for block in re.split(r"(?=^\[submodule )", text, flags=re.M):
            path = re.search(r"^\s*path\s*=\s*(\S+)", block, re.M)
            url = re.search(r"^\s*url\s*=\s*(\S+)", block, re.M)
            if path and not path.group(1).startswith("3rdparty/"):
                self.report("system-dependency", ".gitmodules", 0, f"submodule outside 3rdparty/: {path.group(1)}")
            if path and not url:
                self.report("system-dependency", ".gitmodules", 0, f"submodule without a url: {path.group(1)}")

    # ---- history and pull request rules --------------------------------------
    def check_history(self, base: str, head: str) -> None:
        header = "core/abi/include/nxabi/nxabi.h"
        diff = git("diff", base, head, "--", header)
        text = diff.stdout.decode(errors="replace")
        if diff.returncode == 0 and text and not re.search(r"^\+#define NXABI_VERSION", text, re.M):
            self.report("abi-version", header)
        log = git("log", "--no-merges", "--format=%h %s", f"{base}..{head}")
        if log.returncode == 0:
            for line in log.stdout.decode(errors="replace").splitlines():
                sha, _, subject = line.partition(" ")
                if not CONVENTIONAL.match(subject):
                    self.report("commit-subject", "", 0, f"{sha} {subject}")

    def check_pull_request(self) -> None:
        title = os.environ.get("PR_TITLE", "")
        body = os.environ.get("PR_BODY", "") or ""
        author = os.environ.get("PR_AUTHOR", "")
        if not CONVENTIONAL.match(title):
            self.report("pr-title", "", 0, title)
        if author.endswith("[bot]"):
            return
        body = re.sub(r"<!--.*?-->", "", body, flags=re.S).replace("\r\n", "\n")
        sections = dict(re.findall(r"^###\s*(.+?)\s*\n(.*?)(?=^###\s|\Z)", body, re.M | re.S))
        for name in ("What", "Tested", "Sources", "Checklist"):
            if not sections.get(name, "").strip():
                self.report("pr-template", "", 0, f"section '{name}' is missing or empty")
        checklist = sections.get("Checklist", "")
        for item in re.findall(r"^\s*- \[ \] (.*)$", checklist, re.M):
            self.report("pr-template", "", 0, f"unchecked: {item.strip()}")
        fields = {k.strip().lower(): v.strip() for k, v in re.findall(r"^\s*- \[[xX ]\] ([^:\n]+):(.*)$", checklist, re.M)}
        rules = {
            "depends on": (r"(#\d+([ ,]+#\d+)*|none)", "#N or none"),
            "fixes": (r"(#\d+([ ,]+#\d+)*|none)", "#N or none"),
            "unblocks": (r".+", "the title and its discussion, or none"),
            "gpus tested": (r".+", "each GPU, or n/a"),
            "ai-assisted": (r"(yes|no)", "yes or no"),
        }
        for key, (pattern, expected) in rules.items():
            value = fields.get(key)
            if value is None:
                self.report("pr-template", "", 0, f"checklist line '{key}' is missing")
            elif not re.fullmatch(pattern, value, re.I):
                self.report("pr-template", "", 0, f"'{key}' must be {expected}, got '{value}'")

    # ---- output --------------------------------------------------------------
    def output(self) -> int:
        errors = 0
        annotate = os.environ.get("GITHUB_ACTIONS") == "true"
        for rule, path, line, detail in self.findings:
            level, doc, message = RULES[rule]
            errors += level == "error"
            text = f"{message}" + (f" ({detail})" if detail else "") + f" [see {doc}]"
            where = f"{path or '(pull request)'}{f':{line}' if line else ''}"
            if annotate:
                loc = f" file={path}" + (f",line={line}" if line else "") + "," if path else " "
                print(f"::{level}{loc}title={rule}::{text}")
            else:
                print(f"{where}: {level}: [{rule}] {text}")
        print(f"check_conventions: {len(self.tree.paths)} files, {len(self.findings)} findings, {errors} errors")
        return 1 if errors else 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--base", help="git revision to compare against: commit subjects and the ABI version bump")
    ap.add_argument("--head", help="check this commit's objects instead of the working tree")
    ap.add_argument("--pr", action="store_true", help="also check PR_TITLE and PR_BODY from the environment")
    ap.add_argument("--list-rules", action="store_true")
    args = ap.parse_args()
    if args.list_rules:
        for rule, (level, doc, message) in RULES.items():
            print(f"{rule:<18} {level:<8} {message} [{doc}]")
        return 0
    checker = Checker(GitTree(args.head) if args.head else WorkTree())
    checker.check_files()
    checker.check_runtime()
    checker.check_environment()
    checker.check_submodules()
    if args.base:
        base = git("rev-parse", "--verify", "--quiet", args.base)
        if base.returncode == 0:
            checker.check_history(args.base, args.head or "HEAD")
    if args.pr:
        checker.check_pull_request()
    return checker.output()


if __name__ == "__main__":
    sys.exit(main())
