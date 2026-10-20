#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Progress numbers for the README badges, the Pages site and pull request
comments. Everything is read statically from the repository, so it needs no
build:

  - SDK exports per runtime module (NX_GUEST_FN / NX_GUEST_VAR in runtime/src)
  - open gaps per module (bullets in docs/dev/debt/<module>.md)
  - synthetic corpus cases (tests/corpus/cases.json)
  - relinker patch classes (PatchKind in core/relinker)
  - roadmap milestones (docs/roadmap.json)

Usage:
  tools/site/progress.py OUT_DIR            write progress.json, badges, progress.svg
  tools/site/progress.py --root DIR OUT_DIR  measure another checkout
  tools/site/progress.py --json             print progress.json to stdout
  tools/site/progress.py --compare BASE.json HEAD.json
                                            markdown summary of the differences (empty if none)
"""
from __future__ import annotations

import argparse
import html
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EXPORT_RE = re.compile(r"NX_GUEST_(?:FN|VAR)\([^\"]*\"([^\"]+)\"")
STATUS_COLORS = {"done": "#2da44e", "in_progress": "#d29922", "planned": "#8c959f", "blocked": "#cf222e"}
STATUS_LABELS = {"done": "done", "in_progress": "in progress", "planned": "planned", "blocked": "blocked"}
OPERATORS = {"nw": "operator new", "na": "operator new[]", "dl": "operator delete", "da": "operator delete[]"}


def pretty(symbol: str) -> str:
    """Readable name for display: nested C++ names without parameters, operators, or the C name."""
    if symbol.startswith("_ZN"):
        parts, i = [], 3
        while i < len(symbol) and symbol[i].isdigit():
            j = i
            while symbol[j].isdigit():
                j += 1
            n = int(symbol[i:j])
            parts.append(symbol[j:j + n])
            i = j + n
        return "::".join(parts) + "()" if parts else symbol
    if symbol.startswith("_Z") and symbol[2:4] in OPERATORS:
        rest = symbol[4:]
        variant = {"m": "", "Pv": "", "Pvm": " (sized)", "mRKSt9nothrow_t": " (nothrow)",
                   "PvSt11align_val_t": " (aligned)", "mSt11align_val_t": " (aligned)"}.get(rest, "")
        return OPERATORS[symbol[2:4]] + variant
    return symbol


def collect(root: Path) -> dict:
    modules: dict[str, dict] = {}
    src = root / "runtime" / "src"
    for module_dir in sorted(p for p in src.iterdir() if p.is_dir()) if src.is_dir() else []:
        symbols = []
        for cpp in sorted(module_dir.rglob("*.cpp")):
            symbols += EXPORT_RE.findall(cpp.read_text(encoding="utf-8"))
        modules[module_dir.name] = {"exports": len(symbols), "symbols": sorted(symbols)}

    debt: dict[str, int] = {}
    debt_dir = root / "docs" / "dev" / "debt"
    for md in sorted(debt_dir.glob("*.md")) if debt_dir.is_dir() else []:
        if md.name == "README.md":
            continue
        count = sum(1 for line in md.read_text(encoding="utf-8").splitlines() if line.startswith("- **"))
        debt[md.stem] = count
    for name, info in modules.items():
        info["debt"] = debt.get(name, 0)

    cases_file = root / "tests" / "corpus" / "cases.json"
    cases = [c["name"] for c in json.loads(cases_file.read_text(encoding="utf-8"))["cases"]] if cases_file.exists() else []

    patcher = root / "core" / "relinker" / "include" / "nx" / "relinker" / "patcher.hpp"
    patch_classes = 0
    if patcher.exists():
        body = re.search(r"enum class PatchKind[^{]*\{(.*?)\};", patcher.read_text(encoding="utf-8"), re.S)
        if body:
            patch_classes = len(re.findall(r"^\s*([A-Z]\w*)\s*,", body.group(1), re.M))

    roadmap_file = root / "docs" / "roadmap.json"
    phases = []
    totals = {"done": 0, "in_progress": 0, "planned": 0, "blocked": 0}
    if roadmap_file.exists():
        for phase in json.loads(roadmap_file.read_text(encoding="utf-8"))["phases"]:
            ms = phase["milestones"]
            for m in ms:
                totals[m["status"]] += 1
            phases.append({"id": phase["id"], "title": phase["title"],
                           "done": sum(m["status"] == "done" for m in ms), "total": len(ms), "milestones": ms})

    return {
        "schema": 1,
        "runtime": {"exports": sum(m["exports"] for m in modules.values()), "modules": modules},
        "debt": {"total": sum(debt.values()), "modules": debt},
        "corpus": {"cases": cases},
        "patch_classes": patch_classes,
        "roadmap": {**totals, "total": sum(totals.values()), "phases": phases},
    }


def text_width(text: str) -> float:
    width = 0.0
    for ch in text:
        if ch in "il.,:;|!'":
            width += 3.2
        elif ch in " /()[]":
            width += 4.0
        elif ch.isupper() or ch in "mwMW%":
            width += 8.5
        elif ch.isdigit():
            width += 7.0
        else:
            width += 6.4
    return width


def badge(label: str, value: str, color: str) -> str:
    lw = round(text_width(label) + 12)
    vw = round(text_width(value) + 12)
    w = lw + vw
    label_e, value_e = html.escape(label), html.escape(value)
    return f"""<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="20" role="img" aria-label="{label_e}: {value_e}">
<title>{label_e}: {value_e}</title>
<linearGradient id="s" x2="0" y2="100%"><stop offset="0" stop-color="#bbb" stop-opacity=".1"/><stop offset="1" stop-opacity=".1"/></linearGradient>
<clipPath id="r"><rect width="{w}" height="20" rx="3" fill="#fff"/></clipPath>
<g clip-path="url(#r)"><rect width="{lw}" height="20" fill="#555"/><rect x="{lw}" width="{vw}" height="20" fill="{color}"/><rect width="{w}" height="20" fill="url(#s)"/></g>
<g fill="#fff" text-anchor="middle" font-family="Verdana,Geneva,DejaVu Sans,sans-serif" font-size="11">
<text x="{lw / 2:.1f}" y="15" fill="#010101" fill-opacity=".3">{label_e}</text><text x="{lw / 2:.1f}" y="14">{label_e}</text>
<text x="{lw + vw / 2:.1f}" y="15" fill="#010101" fill-opacity=".3">{value_e}</text><text x="{lw + vw / 2:.1f}" y="14">{value_e}</text>
</g></svg>
"""


def roadmap_svg(data: dict) -> str:
    """One row per phase, one square per milestone, coloured by status."""
    cell, gap, label_w, top = 16, 4, 330, 34
    phases = data["roadmap"]["phases"]
    max_cells = max((p["total"] for p in phases), default=1)
    width = label_w + max_cells * (cell + gap) + 70
    height = top + len(phases) * (cell + 10) + 34
    rows = []
    for i, p in enumerate(phases):
        y = top + i * (cell + 10)
        rows.append(f'<text class="t" x="0" y="{y + 12}">{html.escape(p["title"])}</text>')
        for j, m in enumerate(p["milestones"]):
            x = label_w + j * (cell + gap)
            rows.append(f'<rect x="{x}" y="{y}" width="{cell}" height="{cell}" rx="3" fill="{STATUS_COLORS[m["status"]]}">'
                        f'<title>{html.escape(m["title"])}: {STATUS_LABELS[m["status"]]}</title></rect>')
        rows.append(f'<text class="m" x="{label_w + p["total"] * (cell + gap) + 6}" y="{y + 12}">{p["done"]}/{p["total"]}</text>')
    legend, x = [], 0
    for status, color in STATUS_COLORS.items():
        legend.append(f'<rect x="{x}" y="{height - 20}" width="12" height="12" rx="2" fill="{color}"/>'
                      f'<text class="m" x="{x + 17}" y="{height - 10}">{STATUS_LABELS[status]}</text>')
        x += 17 + round(text_width(STATUS_LABELS[status])) + 22
    r = data["roadmap"]
    return f"""<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}" role="img" aria-label="anyswitch roadmap: {r['done']} of {r['total']} milestones done">
<style>
.t,.m,.h{{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Helvetica,Arial,sans-serif;fill:#1f2328}}
.t{{font-size:13px}} .m{{font-size:12px;fill:#59636e}} .h{{font-size:15px;font-weight:600}}
@media (prefers-color-scheme: dark){{.t,.h{{fill:#e6edf3}} .m{{fill:#9198a1}}}}
</style>
<text class="h" x="0" y="16">anyswitch roadmap: {r['done']} of {r['total']} milestones done</text>
{chr(10).join(rows)}
{''.join(legend)}
</svg>
"""


def write(data: dict, out: Path) -> None:
    out.mkdir(parents=True, exist_ok=True)
    (out / "progress.json").write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    r = data["roadmap"]
    share = r["done"] / r["total"] if r["total"] else 0
    road_color = "#2da44e" if share >= 0.5 else "#d29922" if share >= 0.2 else "#e16f24"
    (out / "badge-exports.svg").write_text(badge("SDK exports", str(data["runtime"]["exports"]), "#0969da"), encoding="utf-8")
    (out / "badge-corpus.svg").write_text(badge("corpus", f"{len(data['corpus']['cases'])} cases", "#2da44e"), encoding="utf-8")
    (out / "badge-roadmap.svg").write_text(badge("roadmap", f"{r['done']}/{r['total']} milestones", road_color), encoding="utf-8")
    (out / "progress.svg").write_text(roadmap_svg(data), encoding="utf-8")


def compare(base: dict, head: dict) -> str:
    lines = []

    def delta(a: int, b: int) -> str:
        return f"{b} ({b - a:+d})" if a != b else str(b)

    rows = []
    if base["runtime"]["exports"] != head["runtime"]["exports"]:
        rows.append(("SDK exports", base["runtime"]["exports"], head["runtime"]["exports"]))
    for name in sorted(set(base["runtime"]["modules"]) | set(head["runtime"]["modules"])):
        a = base["runtime"]["modules"].get(name, {}).get("exports", 0)
        b = head["runtime"]["modules"].get(name, {}).get("exports", 0)
        if a != b:
            rows.append((f"&nbsp;&nbsp;`{name}`", a, b))
    if base["debt"]["total"] != head["debt"]["total"]:
        rows.append(("open gaps (debt items)", base["debt"]["total"], head["debt"]["total"]))
    if len(base["corpus"]["cases"]) != len(head["corpus"]["cases"]):
        rows.append(("corpus cases", len(base["corpus"]["cases"]), len(head["corpus"]["cases"])))
    if base["patch_classes"] != head["patch_classes"]:
        rows.append(("patch classes", base["patch_classes"], head["patch_classes"]))
    if base["roadmap"]["done"] != head["roadmap"]["done"]:
        rows.append(("roadmap milestones done", base["roadmap"]["done"], head["roadmap"]["done"]))
    if rows:
        lines += ["| | before | after |", "|---|---:|---:|"]
        lines += [f"| {label} | {a} | {delta(a, b)} |" for label, a, b in rows]

    old = {m["id"]: m for p in base["roadmap"]["phases"] for m in p["milestones"]}
    changed = [(m, old.get(m["id"], {}).get("status", "new"))
               for p in head["roadmap"]["phases"] for m in p["milestones"]
               if old.get(m["id"], {}).get("status") != m["status"]]
    if changed:
        lines += ["", "Roadmap:"] + [f"- {m['title']}: {s} → {m['status']}" for m, s in changed]

    before = {s for m in base["runtime"]["modules"].values() for s in m["symbols"]}
    after = {s for m in head["runtime"]["modules"].values() for s in m["symbols"]}
    added, removed = sorted(after - before), sorted(before - after)
    if added:
        lines += ["", f"New exports ({len(added)}):"] + [f"- `{pretty(s)}`" for s in added[:40]]
        if len(added) > 40:
            lines.append(f"- … and {len(added) - 40} more")
    if removed:
        lines += ["", f"Removed exports ({len(removed)}):"] + [f"- `{pretty(s)}`" for s in removed[:40]]
    return "\n".join(lines).strip()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--compare", nargs=2, type=Path, metavar=("BASE", "HEAD"))
    ap.add_argument("out", nargs="?", type=Path)
    args = ap.parse_args()
    if args.compare:
        base, head = (json.loads(p.read_text(encoding="utf-8")) for p in args.compare)
        text = compare(base, head)
        if text:
            print(text)
        return 0
    data = collect(args.root.resolve())
    if args.json:
        print(json.dumps(data, indent=2))
        return 0
    if not args.out:
        r = data["roadmap"]
        print(f"SDK exports: {data['runtime']['exports']} in {len(data['runtime']['modules'])} modules")
        print(f"open gaps: {data['debt']['total']}, corpus cases: {len(data['corpus']['cases'])}, "
              f"patch classes: {data['patch_classes']}, roadmap: {r['done']}/{r['total']}")
        return 0
    write(data, args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
