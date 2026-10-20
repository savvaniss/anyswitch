#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Fetches the cross toolchain used to build the AArch64 runtime and the
# synthetic test corpus, without root:
#   - Zig (bundles clang + lld + libc headers) from ziglang.org, checksum-verified
#   - qemu-user (aarch64 user-mode emulator) from the distro archive via apt-get download
#   - an AArch64 glibc sysroot (libc6-arm64-cross) from the distro archive
#
# Only official sources are used. Nothing here touches game data or keys.
# Usage: tools/bootstrap/fetch_toolchain.sh [DEST]   (default: $HOME/.cache/anyswitch/toolchain)
set -euo pipefail

ZIG_VERSION="${ZIG_VERSION:-0.17.0}"
DEST="${1:-${ANYSWITCH_TOOLCHAIN:-$HOME/.cache/anyswitch/toolchain}}"
mkdir -p "$DEST"
DEST="$(cd "$DEST" && pwd)"
DL="$DEST/downloads"
mkdir -p "$DL"

host_arch="$(uname -m)"
case "$host_arch" in
  x86_64) zig_host="x86_64-linux" ;;
  aarch64) zig_host="aarch64-linux" ;;
  *) echo "unsupported host arch: $host_arch" >&2; exit 1 ;;
esac

fetch_zig() {
  if [ -x "$DEST/zig/zig" ] && [ "$("$DEST/zig/zig" version)" = "$ZIG_VERSION" ]; then
    echo "zig $ZIG_VERSION already present"
    return
  fi
  echo "fetching zig $ZIG_VERSION for $zig_host"
  local index="$DL/zig-index.json"
  curl -fsSL https://ziglang.org/download/index.json -o "$index"
  local meta
  meta="$(python3 -I - "$index" "$ZIG_VERSION" "$zig_host" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
e = d[sys.argv[2]][sys.argv[3]]
print(e["tarball"], e["shasum"])
PY
)"
  local url sha
  url="${meta%% *}"
  sha="${meta##* }"
  local tarball
  tarball="$DL/$(basename "$url")"
  curl -fsSL "$url" -o "$tarball"
  echo "$sha  $tarball" | sha256sum -c -
  rm -rf "$DEST/zig" "$DEST/zig.tmp"
  mkdir -p "$DEST/zig.tmp"
  tar -xJf "$tarball" -C "$DEST/zig.tmp" --strip-components=1
  mv "$DEST/zig.tmp" "$DEST/zig"
}

QEMU_PATH="$DEST/qemu/usr/bin/qemu-aarch64"
SYSROOT_PATH="$DEST/sysroot/usr/aarch64-linux-gnu"

fetch_debs() {
  if [ "$host_arch" = "aarch64" ]; then
    echo "native aarch64 host: qemu and cross sysroot not needed"
    QEMU_PATH=""
    SYSROOT_PATH=""
    return
  fi
  if [ -n "${QEMU_AARCH64:-}" ] && [ -n "${AARCH64_SYSROOT:-}" ]; then
    echo "using preinstalled qemu ($QEMU_AARCH64) and sysroot ($AARCH64_SYSROOT)"
    QEMU_PATH="$QEMU_AARCH64"
    SYSROOT_PATH="$AARCH64_SYSROOT"
    return
  fi
  if [ -x "$QEMU_PATH" ] && [ -e "$SYSROOT_PATH/lib/ld-linux-aarch64.so.1" ]; then
    echo "qemu and sysroot already present"
    return
  fi
  command -v apt-get >/dev/null || { echo "apt-get not found; install qemu-user and an aarch64 glibc yourself and set QEMU_AARCH64 and AARCH64_SYSROOT" >&2; exit 1; }
  (cd "$DL" && apt-get download qemu-user libc6-arm64-cross)
  rm -rf "$DEST/qemu" "$DEST/sysroot"
  mkdir -p "$DEST/qemu" "$DEST/sysroot"
  for deb in "$DL"/qemu-user_*.deb; do dpkg-deb -x "$deb" "$DEST/qemu"; done
  for deb in "$DL"/libc6-arm64-cross_*.deb; do dpkg-deb -x "$deb" "$DEST/sysroot"; done
}

fetch_zig
fetch_debs

cat > "$DEST/env.sh" <<ENV
# source this file: . "$DEST/env.sh"
export ANYSWITCH_TOOLCHAIN="$DEST"
export ZIG="$DEST/zig/zig"
export QEMU_AARCH64="$QEMU_PATH"
export AARCH64_SYSROOT="$SYSROOT_PATH"
ENV
echo "toolchain ready: . $DEST/env.sh"
