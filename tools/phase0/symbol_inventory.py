#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Phase 0 measurement (PLAN.md Q4): which SDK functions do titles import,
and how many titles need each one? The answer is the implementation order
for runtime/src/.

Pass one exefs folder per title. With --registry the report marks what the
runtime already provides and ranks the missing symbols by how many titles
import them. Output is aggregate counts only.

Usage:
  tools/phase0/symbol_inventory.py --relink build/host/anyswitch-relink \
      [--registry build/runtime/libnxrt.so] [--top 50] [--csv out.csv] EXEFS_DIR...
"""
from __future__ import annotations

import argparse
import collections
import csv
import json
import subprocess
import sys
from pathlib import Path

MODULE_NAMES = ["main"] + [f"subsdk{i}" for i in range(10)]


def namespace(symbol: str) -> str:
    """nn::os, nn::fs, ... for nested C++ names; nvn / vulkan / libc otherwise."""
    if symbol.startswith("_ZN"):
        parts, i = [], 3
        while i < len(symbol) and symbol[i].isdigit() and len(parts) < 2:
            j = i
            while symbol[j].isdigit():
                j += 1
            n = int(symbol[i:j])
            parts.append(symbol[j:j + n])
            i = j + n
        return "::".join(parts[:2]) if parts else "c++"
    if symbol.startswith("_Z"):
        return "c++ (global)"
    if symbol.startswith("nvn"):
        return "nvn"
    if symbol.startswith("vk"):
        return "vulkan"
    if symbol.startswith("gl") or symbol.startswith("egl"):
        return "opengl"
    return "libc / C"


def run_json(cmd: list[str]):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(r.stderr.strip())
    return json.loads(r.stdout)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--relink", required=True)
    ap.add_argument("--registry")
    ap.add_argument("--top", type=int, default=40)
    ap.add_argument("--csv", type=Path)
    ap.add_argument("titles", nargs="+", help="one exefs folder per title")
    args = ap.parse_args()

    provided: set[str] = set()
    if args.registry:
        r = subprocess.run([args.relink, "provided", args.registry], capture_output=True, text=True, check=True)
        provided = set(r.stdout.split())

    titles_per_symbol: collections.Counter = collections.Counter()
    titles = 0
    for title in map(Path, args.titles):
        mods = [title / n for n in MODULE_NAMES if (title / n).exists()]
        if not mods:
            print(f"skipped {title}: no main/subsdk modules", file=sys.stderr)
            continue
        titles += 1
        exported, imported = set(), set()
        for m in mods:
            s = run_json([args.relink, "symbols", "--json", str(m)])
            exported |= set(s["exports"])
            imported |= set(s["imports"])
        for sym in imported - exported:
            titles_per_symbol[sym] += 1

    by_ns: dict[str, list[str]] = collections.defaultdict(list)
    for sym in titles_per_symbol:
        by_ns[namespace(sym)].append(sym)
    print(f"{titles} titles, {len(titles_per_symbol)} distinct imported symbols\n")
    print(f"{'namespace':<22}{'symbols':>9}{'provided':>10}")
    for ns in sorted(by_ns, key=lambda n: -len(by_ns[n])):
        syms = by_ns[ns]
        print(f"{ns:<22}{len(syms):>9}{sum(s in provided for s in syms):>10}")

    missing = [s for s in titles_per_symbol if s not in provided]
    missing.sort(key=lambda s: (-titles_per_symbol[s], s))
    print(f"\nmost needed missing symbols (titles importing them):")
    for s in missing[:args.top]:
        print(f"  {titles_per_symbol[s]:>4}  {s}")
    if args.csv:
        with args.csv.open("w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["symbol", "namespace", "titles", "provided"])
            for s in sorted(titles_per_symbol, key=lambda s: (-titles_per_symbol[s], s)):
                w.writerow([s, namespace(s), titles_per_symbol[s], s in provided])
    return 0


if __name__ == "__main__":
    sys.exit(main())
