#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Builds the host tools (relinker, unit tests) and the AArch64 runtime.
# Usage: tools/dev/build.sh [build-root]     (default: <repo>/build)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${1:-$ROOT/build}"
TOOLCHAIN_ENV="${ANYSWITCH_TOOLCHAIN:-$HOME/.cache/anyswitch/toolchain}/env.sh"
if [ -f "$TOOLCHAIN_ENV" ]; then
  # shellcheck disable=SC1090
  . "$TOOLCHAIN_ENV"
fi
GEN=()
if command -v ninja >/dev/null 2>&1; then GEN=(-G Ninja); fi
JOBS="$(nproc 2>/dev/null || echo 4)"

cmake -S "$ROOT" -B "$OUT/host" "${GEN[@]}" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DANYSWITCH_WERROR=ON
cmake --build "$OUT/host" -j "$JOBS"

if [ -z "${ZIG:-}" ] && ! command -v zig >/dev/null 2>&1; then
  echo "zig not found: run tools/bootstrap/fetch_toolchain.sh to build the runtime" >&2
  exit 1
fi
cmake -S "$ROOT/runtime" -B "$OUT/runtime" "${GEN[@]}" \
  --toolchain "$ROOT/cmake/toolchains/aarch64-linux-zig.cmake" -DNXRT_WERROR=ON
cmake --build "$OUT/runtime" -j "$JOBS"
echo "built: $OUT/host/anyswitch-relink and $OUT/runtime/libnxrt.so"
