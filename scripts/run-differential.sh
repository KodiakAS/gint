#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT_DIR"

CXX=${CXX:-c++}
BUILD_DIR=${GINT_DIFFERENTIAL_BUILD_DIR:-runs/differential}
ITERATIONS=${GINT_DIFFERENTIAL_ITERATIONS:-512}
mkdir -p "$BUILD_DIR"

"$CXX" \
    -std=c++11 \
    -O2 \
    -g \
    -Wall \
    -Wextra \
    -Werror \
    -Iinclude \
    tests/differential/differential_test.cpp \
    -o "$BUILD_DIR/differential_test"

if [[ $# -gt 0 ]]; then
    "$BUILD_DIR/differential_test" "$@"
else
    "$BUILD_DIR/differential_test" "$ITERATIONS"
fi
