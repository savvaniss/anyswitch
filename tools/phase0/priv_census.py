#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Phase 0 measurement (PLAN.md Q1): how often do title modules contain raw
supervisor calls, system-register accesses and exclusive loads/stores?

Runs `anyswitch-relink census --json` over every module you point it at and
prints aggregate counts. By default the report contains no per-title data,
so it can be shared; --per-module adds a row per input file.

Usage:
  tools/phase0/priv_census.py --relink build/host/anyswitch-relink EXEFS_DIR_OR_MODULE...
"""
from __future__ import annotations

import argparse
import collections
import json
import subprocess
import sys
from pathlib import Path

MODULE_NAMES = ["main"] + [f"subsdk{i}" for i in range(10)]


def modules(paths: list[str]) -> list[Path]:
    out = []
    for p in map(Path, paths):
        if p.is_dir():
            out += [p / n for n in MODULE_NAMES if (p / n).exists()]
        else:
            out.append(p)
    return out


def census(relink: str, path: Path) -> dict:
    r = subprocess.run([relink, "census", "--json", str(path)], capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"{path}: {r.stderr.strip()}")
    return json.loads(r.stdout)[0]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--relink", required=True)
    ap.add_argument("--per-module", action="store_true")
    ap.add_argument("--json", type=Path, help="also write the aggregate as JSON")
    ap.add_argument("inputs", nargs="+")
    args = ap.parse_args()

    totals: dict[str, collections.Counter] = collections.defaultdict(collections.Counter)
    affected: dict[str, collections.Counter] = collections.defaultdict(collections.Counter)
    count = collections.Counter()
    for path in modules(args.inputs):
        kind = "main" if path.name == "main" else "subsdk"
        try:
            c = census(args.relink, path)
        except RuntimeError as e:
            print(f"skipped: {e}", file=sys.stderr)
            continue
        count[kind] += 1
        rows = {}
        for num, n in c["svc"].items():
            rows[f"svc {num}"] = n
        for group, prefix in (("sysreg_reads", "mrs "), ("sysreg_writes", "msr "), ("cache_ops", ""),
                              ("exceptions", "")):
            for k, n in c[group].items():
                rows[prefix + k] = n
        rows["exclusive (unordered)"] = c["exclusive_unordered"]
        for k, n in c["relocations"].items():
            rows[k] = n
        for k, n in rows.items():
            if n:
                totals[kind][k] += n
                affected[kind][k] += 1
        if args.per_module:
            print(f"{path}: " + ", ".join(f"{k}={n}" for k, n in sorted(rows.items()) if n))

    for kind in ("main", "subsdk"):
        if not count[kind]:
            continue
        print(f"\n{kind}: {count[kind]} modules")
        print(f"  {'item':<34}{'modules':>9}{'total':>12}")
        for k in sorted(totals[kind], key=lambda k: (-affected[kind][k], k)):
            print(f"  {k:<34}{affected[kind][k]:>9}{totals[kind][k]:>12}")
    if args.json:
        args.json.write_text(json.dumps({k: {"modules": count[k], "affected": affected[k], "totals": totals[k]}
                                         for k in ("main", "subsdk")}, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
