#!/usr/bin/env bash

set -euo pipefail

if [[ $# -gt 2 ]]; then
    echo "usage: $0 [compiler] [output-directory]" >&2
    exit 2
fi

CXX_BIN="${1:-${CXX:-c++}}"
OUTPUT_DIR="${2:-runs/performance/codegen}"
ASSEMBLY="$OUTPUT_DIR/codegen_contract.s"
REPORT="$OUTPUT_DIR/codegen_contract.json"

mkdir -p "$OUTPUT_DIR"

COMPILER_VERSION="$("$CXX_BIN" --version | sed -n '1p')"
TARGET="$("$CXX_BIN" -dumpmachine)"

"$CXX_BIN" \
    -std=c++11 \
    -O3 \
    -DNDEBUG \
    -Wall \
    -Wextra \
    -Werror \
    -fno-stack-protector \
    -fno-optimize-sibling-calls \
    -Iinclude \
    -S tests/perf/codegen_contract.cpp \
    -o "$ASSEMBLY"

python3 scripts/check_codegen_contract.py \
    --assembly "$ASSEMBLY" \
    --contract tests/perf/codegen_contract.json \
    --compiler "$COMPILER_VERSION" \
    --target "$TARGET" \
    --output "$REPORT"
