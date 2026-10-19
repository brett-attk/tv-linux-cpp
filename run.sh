#!/usr/bin/env bash
# Prefer X11 so libmpv can embed via window id (needed under Wayland).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
BIN="${ROOT}/build/tv-cpp"
if [[ ! -x "$BIN" ]]; then
  echo "Binary not found: $BIN" >&2
  echo "Build first: cmake -B build && cmake --build build" >&2
  exit 1
fi
export GDK_BACKEND="${GDK_BACKEND:-x11}"
exec "$BIN" "$@"
