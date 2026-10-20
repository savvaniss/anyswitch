#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Builds the GitHub Pages site: home page, progress page, badges and the
roadmap map. Deployed by .github/workflows/pages.yml.

Usage:
  tools/site/build_site.py OUT_DIR [--repo owner/name] [--commit SHA]
"""
from __future__ import annotations

import argparse
import html
import os
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import progress  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]


def page(title: str, active: str, body: str, repo: str, commit: str) -> str:
    gh = f"https://github.com/{repo}"
    nav = [("index.html", "Home"), ("progress.html", "Progress")]
    links = "".join(f'<a href="{href}"{" class=\"active\"" if href == active else ""}>{label}</a>' for href, label in nav)
    links += (f'<a href="{gh}/blob/main/AGENTS.md">Contribute</a>'
              f'<a href="{gh}/discussions">Discussions</a><a href="{gh}">GitHub</a>')
    built = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M UTC")
    short = html.escape(commit[:7]) if commit else "local"
    commit_link = f'<a href="{gh}/commit/{html.escape(commit)}">{short}</a>' if commit else short
    return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{html.escape(title)}</title>
<meta name="description" content="anyswitch converts Switch executables you already have in plain form into native AArch64 programs with a native runtime in place of the SDK. No emulator.">
<link rel="stylesheet" href="style.css">
<link rel="icon" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'%3E%3Crect width='16' height='16' rx='3' fill='%230969da'/%3E%3Ctext x='8' y='12' font-size='10' text-anchor='middle' fill='white' font-family='sans-serif'%3En%3C/text%3E%3C/svg%3E">
</head>
<body>
<header class="site"><div class="wrap"><a class="brand" href="index.html">any<span>switch</span></a><nav>{links}</nav></div></header>
<main><div class="wrap">
{body}
</div></main>
<footer><div class="wrap">MIT licensed code. It grants nothing for any game, game data or firmware. anyswitch reads only plain executables you already have and never handles keys or encrypted content.
Built {built} from {commit_link}.</div></footer>
</body>
</html>
"""


def badges(repo: str) -> str:
    gh = f"https://github.com/{repo}"
    return (f'<div class="badges"><a href="{gh}/actions/workflows/ci.yml"><img alt="CI" src="{gh}/actions/workflows/ci.yml/badge.svg"></a>'
            '<a href="progress.html"><img alt="SDK exports" src="badge-exports.svg"></a>'
            '<a href="progress.html#corpus"><img alt="corpus" src="badge-corpus.svg"></a>'
            '<a href="progress.html#roadmap"><img alt="roadmap" src="badge-roadmap.svg"></a></div>')


def index_body(data: dict, repo: str) -> str:
    gh = f"https://github.com/{repo}"
    r = data["roadmap"]
    phase_rows = "".join(
        f'<tr><td>{html.escape(p["title"])}</td><td class="num">{p["done"]}/{p["total"]}</td>'
        f'<td><div class="bar"><i style="width:{100 * p["done"] / p["total"]:.0f}%"></i></div></td></tr>'
        for p in r["phases"])
    return f"""
<h1>Switch executables, relinked to run natively</h1>
<p class="lead">anyswitch converts Switch executables you already have in plain form into native AArch64 Linux programs, and supplies a native runtime in place of the title's SDK. No emulator: the original machine code runs in-process after a handful of instructions are patched.</p>
{badges(repo)}
<div class="callout warn"><b>Early development.</b> The relinker, the runtime core and the measurement tools work end to end on a synthetic test corpus. No commercial title runs yet. See the <a href="progress.html">progress page</a>.</div>

<h2>How it works</h2>
<div class="cards">
  <div class="card"><h3>Relink</h3><p><code>anyswitch-relink</code> reads NSO and NRO modules, rebases them into a host ELF, prefixes every imported and exported symbol, and patches the few instructions that cannot run in a normal process: supervisor calls, thread and timer registers, exclusive loads and stores.</p></div>
  <div class="card"><h3>Replace the SDK</h3><p><code>libnxrt.so</code> exports native implementations of the SDK functions titles import by name. A missing function stops the program and names itself, so every gap is visible.</p></div>
  <div class="card"><h3>Run natively</h3><p>On AArch64 Linux the converted program starts like any other. On x86-64, the test corpus runs under qemu-user today; a static recompiler is planned for later phases.</p></div>
</div>

<h2>Input contract</h2>
<div class="callout">anyswitch reads only NSO, NRO and NPDM files from an <code>exefs</code> folder you already have in plain, unpackaged form, from software you own. It does not read keys, does not process encrypted content, does not unpack packaged containers, and the project does not explain how to obtain any of those things. Packaged or encrypted input is refused.</div>

<h2>Get started</h2>
<pre><code>git clone {gh}.git
cd anyswitch
tools/bootstrap/fetch_toolchain.sh   # Zig, qemu-user, AArch64 sysroot; no root needed
tools/dev/build.sh                   # relinker and libnxrt.so
tools/dev/test.sh                    # unit tests and the synthetic corpus</code></pre>
<p>Then see <a href="{gh}/blob/main/docs/user/USAGE.md">USAGE.md</a> for converting your own title and reading the exit statuses.</p>

<h2 id="roadmap">Roadmap</h2>
<p>{r["done"]} of {r["total"]} milestones done. Details on the <a href="progress.html#roadmap">progress page</a>.</p>
<table><thead><tr><th>Phase</th><th class="num">Done</th><th>Progress</th></tr></thead><tbody>{phase_rows}</tbody></table>

<h2>Contribute</h2>
<div class="cards">
  <div class="card"><h3><a href="{gh}/blob/main/AGENTS.md">AGENTS.md</a></h3><p>The rulebook for people and AI coding agents: legal boundaries, engineering rules, repository map, workflow.</p></div>
  <div class="card"><h3><a href="{gh}/tree/main/docs/dev/debt">Known gaps</a></h3><p>One file per module listing what is missing. The best place to find work.</p></div>
  <div class="card"><h3><a href="{gh}/blob/main/docs/dev/RECIPES.md">Recipes</a></h3><p>Step by step: add an SDK function, an SVC, a corpus case, a patch class.</p></div>
  <div class="card"><h3><a href="{gh}/discussions">Discussions</a></h3><p>Title bring-up threads: how far a title gets, without sharing any of its data.</p></div>
</div>

<h2>Background</h2>
<p><a href="{gh}/blob/main/docs/FEASIBILITY.md">Feasibility study</a> · <a href="{gh}/blob/main/docs/PLAN.md">Plan</a> · <a href="{gh}/blob/main/docs/REUSE.md">Reuse survey</a> · <a href="{gh}/blob/main/docs/dev/ARCHITECTURE.md">Architecture</a></p>
"""


def progress_body(data: dict, repo: str) -> str:
    gh = f"https://github.com/{repo}"
    r = data["roadmap"]
    rt = data["runtime"]
    stats = [
        (rt["exports"], "SDK exports"),
        (len(rt["modules"]), "runtime modules"),
        (data["debt"]["total"], "open gaps"),
        (len(data["corpus"]["cases"]), "corpus cases"),
        (data["patch_classes"], "patch classes"),
        (f'{r["done"]}/{r["total"]}', "milestones done"),
    ]
    stat_html = "".join(f'<div class="stat"><b>{v}</b><span>{label}</span></div>' for v, label in stats)

    phases = []
    for p in r["phases"]:
        items = "".join(f'<li><span>{html.escape(m["title"])}</span>'
                        f'<span class="pill {m["status"]}">{progress.STATUS_LABELS[m["status"]]}</span></li>'
                        for m in p["milestones"])
        phases.append(f'<h3>{html.escape(p["title"])} <small>({p["done"]}/{p["total"]})</small></h3><ul class="milestones">{items}</ul>')

    module_rows = []
    debt = data["debt"]["modules"]
    for name in sorted(set(rt["modules"]) | set(debt)):
        info = rt["modules"].get(name, {"exports": 0, "symbols": []})
        names = "".join(f"<li><code>{html.escape(progress.pretty(s))}</code></li>" for s in info["symbols"])
        listing = f"<details><summary>{info['exports']} symbols</summary><ul>{names}</ul></details>" if names else "—"
        where = "runtime/src/" + name if name in rt["modules"] else "core/"
        module_rows.append(
            f'<tr><td><a href="{gh}/tree/main/{where}"><code>{html.escape(name)}</code></a></td>'
            f'<td class="num">{info["exports"]}</td>'
            f'<td class="num"><a href="{gh}/blob/main/docs/dev/debt/{html.escape(name)}.md">{debt.get(name, 0)}</a></td>'
            f"<td>{listing}</td></tr>")

    cases = "".join(f'<li><a href="{gh}/blob/main/tests/corpus/cases.json"><code>{html.escape(c)}</code></a></li>'
                    for c in data["corpus"]["cases"])
    return f"""
<h1>Progress</h1>
<p class="lead">Generated from the repository on every push to <code>main</code>. Raw numbers: <a href="progress.json">progress.json</a>.</p>
<div class="grid">{stat_html}</div>

<h2 id="roadmap">Roadmap</h2>
<div class="map"><img src="progress.svg" alt="Roadmap map: {r['done']} of {r['total']} milestones done"></div>
{''.join(phases)}
<p>Milestones come from <a href="{gh}/blob/main/docs/roadmap.json">docs/roadmap.json</a>, which follows the phases in <a href="{gh}/blob/main/docs/PLAN.md">PLAN.md</a>.</p>

<h2 id="sdk">SDK coverage</h2>
<p>An export is a guest-visible symbol <code>libnxrt.so</code> provides. There is no complete list of SDK functions to measure against, so the honest numbers are counts: what exists and what is known to be missing. Per-title needs come from <code>tools/phase0/symbol_inventory.py</code> run by people on titles they own.</p>
<table><thead><tr><th>Module</th><th class="num">Exports</th><th class="num">Open gaps</th><th>Symbols</th></tr></thead>
<tbody>{''.join(module_rows)}</tbody></table>

<h2 id="corpus">Synthetic corpus</h2>
<p>Guest programs built from source into NSO and NRO modules, relinked and run against the runtime in CI on x86-64 (qemu-user) and native AArch64. Every runtime export must be referenced by at least one case.</p>
<ul>{cases}</ul>
"""


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out", type=Path)
    ap.add_argument("--repo", default=os.environ.get("GITHUB_REPOSITORY", "savvaniss/anyswitch"))
    ap.add_argument("--commit", default=os.environ.get("GITHUB_SHA", ""))
    args = ap.parse_args()
    if not args.commit:
        try:
            args.commit = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True,
                                         check=True).stdout.strip()
        except (OSError, subprocess.CalledProcessError):
            args.commit = ""

    out = args.out
    out.mkdir(parents=True, exist_ok=True)
    data = progress.collect(ROOT)
    progress.write(data, out)
    shutil.copy2(ROOT / "site" / "style.css", out / "style.css")
    (out / ".nojekyll").write_text("", encoding="utf-8")
    (out / "index.html").write_text(page("anyswitch", "index.html", index_body(data, args.repo), args.repo, args.commit),
                                    encoding="utf-8")
    (out / "progress.html").write_text(page("anyswitch progress", "progress.html", progress_body(data, args.repo),
                                            args.repo, args.commit), encoding="utf-8")
    (out / "404.html").write_text(page("Not found", "", '<h1>Not found</h1><p><a href="index.html">Home</a></p>',
                                       args.repo, args.commit), encoding="utf-8")
    print(f"site written to {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
