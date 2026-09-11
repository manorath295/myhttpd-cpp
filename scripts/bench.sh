#!/usr/bin/env bash
# scripts/bench.sh — run the in-process benchmark and pretty-print the result.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

THREADS="${THREADS:-4}"
REQUESTS="${REQUESTS:-500}"

if [ ! -x "$BUILD_DIR/myhttpd_bench" ]; then
    echo ">>> building benchmark..."
    cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$BUILD_DIR" --target myhttpd_bench -j
fi

"$BUILD_DIR/myhttpd_bench" "$THREADS" "$REQUESTS"
