#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT_DIR"

CXX=${CXX:-clang++}
BUILD_DIR=${GINT_FUZZ_BUILD_DIR:-runs/fuzz}
FUZZ_SECONDS=${1:-${GINT_FUZZ_SECONDS:-30}}
CORPUS_DIR="$BUILD_DIR/corpus"
ARTIFACT_DIR="$BUILD_DIR/artifacts"
mkdir -p "$BUILD_DIR" "$CORPUS_DIR" "$ARTIFACT_DIR"
cp -R tests/fuzz/corpus/. "$CORPUS_DIR/"

if [[ "$(uname -s)" == "Darwin" ]]; then
    RESOURCE_DIR=$("$CXX" -print-resource-dir 2>/dev/null || true)
    FUZZER_RUNTIME_FOUND=0
    for runtime in "$RESOURCE_DIR"/lib/darwin/libclang_rt.fuzzer*.a; do
        if [[ -f "$runtime" ]]; then
            FUZZER_RUNTIME_FOUND=1
            break
        fi
    done
    if [[ $FUZZER_RUNTIME_FOUND -eq 0 ]]; then
        DEVELOPER_DIR_PATH=$(xcode-select -p 2>/dev/null || true)
        if [[ "$DEVELOPER_DIR_PATH" == *CommandLineTools* ]]; then
            printf '%s\n' \
                "error: Apple Command Line Tools Clang does not include the libFuzzer runtime." \
                "compiler: $CXX" \
                "resource directory: ${RESOURCE_DIR:-unavailable}" \
                "Install and select full Xcode, or run scripts/run-fuzz.sh with Linux Clang." \
                "The deterministic oracle remains available via scripts/run-differential.sh." >&2
        else
            printf '%s\n' \
                "error: the selected Darwin Clang does not provide the libFuzzer runtime." \
                "compiler: $CXX" \
                "resource directory: ${RESOURCE_DIR:-unavailable}" >&2
        fi
        exit 2
    fi
fi

"$CXX" \
    -std=c++11 \
    -O1 \
    -g \
    -fno-omit-frame-pointer \
    -Wall \
    -Wextra \
    -Werror \
    -fsanitize=fuzzer,address,undefined \
    -Iinclude \
    tests/fuzz/gint_fuzz.cpp \
    -o "$BUILD_DIR/gint_fuzz"

"$BUILD_DIR/gint_fuzz" \
    "$CORPUS_DIR" \
    -dict=tests/fuzz/gint.dict \
    -artifact_prefix="$ARTIFACT_DIR/" \
    -max_len=4096 \
    -max_total_time="$FUZZ_SECONDS" \
    -print_final_stats=1 \
    -rss_limit_mb=2048 \
    -timeout=10 \
    -verbosity=0
