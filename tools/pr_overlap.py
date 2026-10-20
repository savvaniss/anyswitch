#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Finds open pull requests that change the same lines or add the same guest
exports, so their authors can agree which goes first (CONTRIBUTING.md).

  tools/pr_overlap.py --branch BRANCH [--base origin/main]   before opening a pull request
  tools/pr_overlap.py --all --out DIR                         CI: DIR/<number>.md per open pull request,
                                                              empty when it overlaps nothing

Needs an authenticated GitHub CLI (`gh`) to list open pull requests.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

MARKER = "<!-- pr-overlap -->"
HUNK = re.compile(r"^@@ -(\d+)(?:,(\d+))? \+\d+(?:,\d+)? @@")
EXPORT = re.compile(r"NX_GUEST_(?:FN|VAR)\(\s*[^,]+,\s*\w+\s*,\s*\"([^\"]+)\"")


@dataclass
class Change:
    label: str
    url: str = ""
    number: int = 0
    ranges: dict[str, list[tuple[int, int]]] = field(default_factory=dict)
    exports: set[str] = field(default_factory=set)


def run(cmd: list[str]) -> str:
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit(f"{' '.join(cmd)} failed: {r.stderr.strip()}")
    return r.stdout


def parse_patch(change: Change, path: str, patch: str) -> None:
    for line in patch.splitlines():
        m = HUNK.match(line)
        if m:
            start = int(m.group(1))
            count = int(m.group(2)) if m.group(2) is not None else 1
            change.ranges.setdefault(path, []).append((start, start + max(count, 1) - 1))
        elif line.startswith("+") and not line.startswith("+++"):
            change.exports.update(EXPORT.findall(line))


def local_change(branch: str, base: str) -> Change:
    change = Change(f"branch {branch}")
    diff = run(["git", "diff", "-U0", "--no-color", f"{base}...{branch}"])
    path = None
    chunk: list[str] = []
    for line in diff.splitlines() + ["diff --git end"]:
        if line.startswith("diff --git"):
            if path and chunk:
                parse_patch(change, path, "\n".join(chunk))
            path, chunk = None, []
        elif line.startswith("+++ "):
            path = line[6:] if line.startswith("+++ b/") else None
        elif path is not None:
            chunk.append(line)
    return change


def open_pull_requests(repo: str | None) -> list[Change]:
    repo_args = ["--repo", repo] if repo else []
    prs = json.loads(run(["gh", "pr", "list", *repo_args, "--state", "open", "--base", "main", "--limit", "200",
                          "--json", "number,title,author,url"]))
    changes = []
    api_repo = repo or run(["gh", "repo", "view", "--json", "nameWithOwner", "--jq", ".nameWithOwner"]).strip()
    for pr in prs:
        change = Change(f"#{pr['number']} {pr['title']} (@{pr['author']['login']})", pr["url"], pr["number"])
        files = json.loads(run(["gh", "api", "--paginate", f"repos/{api_repo}/pulls/{pr['number']}/files"]))
        for f in files:
            if f.get("patch"):
                parse_patch(change, f["filename"], f["patch"])
            else:
                change.ranges.setdefault(f["filename"], []).append((1, 1 << 30))
        changes.append(change)
    return changes


def overlap(a: Change, b: Change) -> list[str]:
    found = []
    for path in sorted(set(a.ranges) & set(b.ranges)):
        hits = [(max(s1, s2), min(e1, e2)) for s1, e1 in a.ranges[path] for s2, e2 in b.ranges[path]
                if s1 <= e2 and s2 <= e1]
        if hits:
            lines = ", ".join(f"{s}" if s == e else f"{s}-{e}" for s, e in sorted(set(hits))[:5] if e < 1 << 29)
            found.append(f"`{path}`" + (f" lines {lines}" if lines else " (whole file)"))
    shared = sorted(a.exports & b.exports)
    if shared:
        found.append("exports " + ", ".join(f"`{s}`" for s in shared[:10]))
    return found


def report(subject: Change, others: list[Change]) -> str:
    rows = []
    for other in others:
        hits = overlap(subject, other)
        if hits:
            rows.append(f"- [{other.label}]({other.url}): " + "; ".join(hits))
    if not rows:
        return ""
    return "\n".join([
        MARKER,
        "### Overlapping pull requests",
        "",
        "These open pull requests change the same lines or add the same exports. Agree with their authors which "
        "one goes first and record it on the `Depends on` line of the later one.",
        "",
        *rows,
    ]) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    mode = ap.add_mutually_exclusive_group(required=True)
    mode.add_argument("--branch")
    mode.add_argument("--all", action="store_true")
    ap.add_argument("--base", default="origin/main")
    ap.add_argument("--repo", help="owner/name; default: the current repository")
    ap.add_argument("--out", type=Path)
    args = ap.parse_args()

    prs = open_pull_requests(args.repo)
    if args.branch:
        text = report(local_change(args.branch, args.base), prs)
        print(text or f"no open pull request overlaps {args.branch}")
        return 0
    if not args.out:
        ap.error("--all needs --out")
    args.out.mkdir(parents=True, exist_ok=True)
    for pr in prs:
        others = [o for o in prs if o is not pr]
        (args.out / f"{pr.number}.md").write_text(report(pr, others), encoding="utf-8")
    print(f"checked {len(prs)} open pull requests")
    return 0


if __name__ == "__main__":
    sys.exit(main())
