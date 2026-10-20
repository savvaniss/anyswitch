#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Runs every test: host unit tests, then the synthetic corpus end to end.
# Usage: tools/dev/test.sh [build-root] [corpus case ...]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${1:-$ROOT/build}"
shift || true
TOOLCHAIN_ENV="${ANYSWITCH_TOOLCHAIN:-$HOME/.cache/anyswitch/toolchain}/env.sh"
if [ -f "$TOOLCHAIN_ENV" ]; then
  # shellcheck disable=SC1090
  . "$TOOLCHAIN_ENV"
fi
(cd "$OUT/host" && ctest --output-on-failure)
python3 -I "$ROOT/tools/corpus/run_corpus.py" --relink "$OUT/host/anyswitch-relink" \
  --runtime "$OUT/runtime/libnxrt.so" --out "$OUT/corpus" --require-coverage "$@"
