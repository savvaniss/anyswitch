#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Builds the synthetic corpus, relinks it and runs it against the runtime.

Every case in tests/corpus/cases.json is compiled from source into NSO/NRO
modules (no game data is involved anywhere), converted with anyswitch-relink,
then executed natively on an AArch64 Linux host or under qemu-aarch64.

Usage:
  tools/corpus/run_corpus.py --relink build/host/anyswitch-relink \
      --runtime build/runtime/libnxrt.so [--out build/corpus] [case ...]

Environment (set by tools/bootstrap/fetch_toolchain.sh's env.sh):
  ZIG              zig binary used to compile the guest modules
  QEMU_AARCH64     user-mode emulator, needed on non-AArch64 hosts
  AARCH64_SYSROOT  AArch64 glibc root for qemu (-L)
"""
from __future__ import annotations

import argparse
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import nxo  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
CORPUS = ROOT / "tests" / "corpus"
TIME_BUDGET = 30   # seconds per case on CI runners, qemu-user included (CONTRIBUTING.md)
HARD_TIMEOUT = 120
GUEST_CFLAGS = [
    "-target", "aarch64-freestanding-none", "-O2", "-fPIC", "-ffreestanding", "-fno-exceptions", "-fno-rtti",
    "-fno-stack-protector", "-mno-outline-atomics", "-fno-sized-deallocation", "-std=c++20",
    "-I", str(CORPUS / "include"),
]


def run(cmd: list[str], **kw) -> subprocess.CompletedProcess:
    result = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if result.returncode != 0:
        sys.stderr.write(f"command failed: {' '.join(cmd)}\n{result.stdout}{result.stderr}")
        raise SystemExit(1)
    return result


def build_module(zig: str, name: str, spec: dict, work: Path, exefs: Path) -> Path:
    work.mkdir(parents=True, exist_ok=True)
    objects = []
    crt0 = work / "crt0.o"
    run([zig, "cc", "-target", "aarch64-freestanding-none", "-c", str(CORPUS / "rt" / "crt0.S"), "-o", str(crt0)])
    objects.append(crt0)
    for src in ["../rt/support.cpp", *spec["sources"]]:
        obj = work / (Path(src).stem + ".o")
        run([zig, "c++", *GUEST_CFLAGS, "-c", str((CORPUS / "src" / src).resolve()), "-o", str(obj)])
        objects.append(obj)
    elf = work / f"{name}.elf"
    run([zig, "ld.lld", "-shared", "-T", str(CORPUS / "rt" / "module.ld"), "--hash-style=sysv", "--eh-frame-hdr",
         "--build-id=sha1", "-z", "norelro", "-z", "max-page-size=0x1000", "-z", "noexecstack",
         "-o", str(elf), *map(str, objects)])
    module = nxo.read_elf(elf.read_bytes())
    data = nxo.write_nro(module) if spec.get("format") == "nro" else nxo.write_nso(module, compress=True)
    out = exefs / name
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(data)
    return out


def check(case: dict, proc: subprocess.CompletedProcess) -> list[str]:
    problems = []
    if proc.returncode != case.get("exit", 0):
        problems.append(f"exit code {proc.returncode}, expected {case.get('exit', 0)}")
    lines = proc.stdout.splitlines()
    at = 0
    for pattern in case.get("stdout", []):
        rx = re.compile(pattern)
        while at < len(lines) and not rx.search(lines[at]):
            at += 1
        if at == len(lines):
            problems.append(f"stdout: no line matching /{pattern}/ (in order)")
            break
        at += 1
    for text in case.get("stdout_absent", []):
        if text in proc.stdout:
            problems.append(f"stdout unexpectedly contains '{text}'")
    for pattern in case.get("stderr", []):
        if not re.search(pattern, proc.stderr):
            problems.append(f"stderr: no match for /{pattern}/")
    return problems


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--relink", required=True, type=Path)
    ap.add_argument("--runtime", required=True, type=Path)
    ap.add_argument("--out", type=Path, default=ROOT / "build" / "corpus")
    ap.add_argument("--zig", default=os.environ.get("ZIG", "zig"))
    ap.add_argument("--qemu", default=os.environ.get("QEMU_AARCH64"))
    ap.add_argument("--sysroot", default=os.environ.get("AARCH64_SYSROOT"))
    ap.add_argument("-v", "--verbose", action="store_true")
    ap.add_argument("--require-coverage", action="store_true",
                    help="fail if a runtime export is referenced by no corpus case (AGENTS.md E4)")
    ap.add_argument("cases", nargs="*")
    args = ap.parse_args()
    args.out = args.out.resolve()
    args.relink = args.relink.resolve()
    args.runtime = args.runtime.resolve()

    spec = json.loads((CORPUS / "cases.json").read_text())
    program_id = int(spec["program_id"], 16)
    native = platform.machine() in ("aarch64", "arm64")
    if not native and not (args.qemu and args.sysroot):
        sys.stderr.write("not an AArch64 host: set QEMU_AARCH64 and AARCH64_SYSROOT (tools/bootstrap/fetch_toolchain.sh)\n")
        return 2

    failures = 0
    selected = [c for c in spec["cases"] if not args.cases or c["name"] in args.cases]
    for case in selected:
        name = case["name"]
        work = args.out / name
        shutil.rmtree(work, ignore_errors=True)
        for module, mspec in case["modules"].items():
            build_module(args.zig, module, mspec, work / "obj" / module, work / "exefs")
        (work / "exefs" / "main.npdm").write_bytes(nxo.write_npdm(program_id))
        dest = work / "app"
        relink = run([str(args.relink), "exefs", str(work / "exefs"), "-o", str(dest),
                      "--registry", str(args.runtime)])
        (dest / "lib").mkdir(parents=True, exist_ok=True)
        shutil.copy2(args.runtime, dest / "lib" / args.runtime.name)
        cmd = [str(dest / "main"), *case.get("args", [])]
        if not native:
            cmd = [args.qemu, "-L", args.sysroot, *cmd]
        started = time.monotonic()
        try:
            proc = subprocess.run(cmd, capture_output=True, text=True, timeout=HARD_TIMEOUT)
        except subprocess.TimeoutExpired as e:
            proc = subprocess.CompletedProcess(cmd, -1, e.stdout or "", e.stderr or "")
        elapsed = time.monotonic() - started
        problems = check(case, proc)
        if proc.returncode == -1:
            problems.append(f"killed after {HARD_TIMEOUT} s")
        status = "PASS" if not problems else "FAIL"
        print(f"{status} {name} ({elapsed:.1f} s)")
        if elapsed > TIME_BUDGET:
            note = f"corpus case {name} took {elapsed:.0f} s; cases must finish in {TIME_BUDGET} s (CONTRIBUTING.md)"
            print(f"::warning title=slow corpus case::{note}" if os.environ.get("GITHUB_ACTIONS") == "true"
                  else f"  warning: {note}")
        if problems or args.verbose:
            for p in problems:
                print(f"  {p}")
            print("  --- relink ---\n" + "".join(f"  {l}\n" for l in relink.stdout.splitlines()))
            print("  --- stdout ---\n" + "".join(f"  {l}\n" for l in proc.stdout.splitlines()))
            print("  --- stderr ---\n" + "".join(f"  {l}\n" for l in proc.stderr.splitlines()))
        failures += bool(problems)
    print(f"{len(selected) - failures}/{len(selected)} corpus cases passed")

    if not args.cases:
        provided = set(run([str(args.relink), "provided", str(args.runtime)]).stdout.split())
        imported: set[str] = set()
        for case in selected:
            for module in case["modules"]:
                path = args.out / case["name"] / "exefs" / module
                imported |= set(json.loads(run([str(args.relink), "symbols", "--json", str(path)]).stdout)["imports"])
        untested = sorted(provided - imported)
        print(f"guest export coverage: {len(provided) - len(untested)}/{len(provided)} referenced by the corpus")
        for name in untested:
            print(f"  not referenced by any case: {name}")
        if untested and args.require_coverage:
            return 1
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
