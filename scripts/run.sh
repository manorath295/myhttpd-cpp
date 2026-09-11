#!/usr/bin/env bash
# scripts/run.sh — build & run myhttpd with sensible defaults.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

PORT="${PORT:-8080}"
ROOTDIR="${ROOTDIR:-$ROOT_DIR/files}"
WORKERS="${WORKERS:-8}"
POLICY="${POLICY:-FCFS}"
LOGFILE="${LOGFILE:-}"

cd "$ROOT_DIR"

if [ ! -d "$BUILD_DIR" ] || [ ! -x "$BUILD_DIR/myhttpd" ]; then
    echo ">>> build/ not found or myhttpd not built; running cmake build..."
    cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build "$BUILD_DIR" -j
fi

ARGS=(
    -p "$PORT"
    -r "$ROOTDIR"
    -n "$WORKERS"
    -s "$POLICY"
    -v info
)
if [ -n "$LOGFILE" ]; then ARGS+=( -l "$LOGFILE" ); fi

echo ">>> starting: $BUILD_DIR/myhttpd ${ARGS[*]}"
exec "$BUILD_DIR/myhttpd" "${ARGS[@]}"
