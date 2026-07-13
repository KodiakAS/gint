#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT_DIR"

RUN_ROOT=${GINT_GCC48_RUN_ROOT:-runs/gcc48}
JOBS=${GINT_GCC48_JOBS:-}
DIFFERENTIAL_ITERATIONS=${GINT_GCC48_DIFFERENTIAL_ITERATIONS:-512}

if [[ -z "$JOBS" ]]; then
    JOBS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)
fi

if [[ "$(uname -s)" != "Linux" || "$(uname -m)" != "x86_64" ]]; then
    echo "error: GCC 4.8.5 validation requires Linux x86_64" >&2
    exit 2
fi

for compiler in gcc g++; do
    if ! command -v "$compiler" >/dev/null 2>&1; then
        echo "error: required compiler not found: $compiler" >&2
        exit 2
    fi
    if [[ "$("$compiler" -dumpversion)" != "4.8.5" ]]; then
        echo "error: $compiler 4.8.5 is required, found $("$compiler" -dumpversion)" >&2
        exit 2
    fi
done

if [[ "$(cmake --version | sed -n '1s/^cmake version //p')" != "3.13.5" ]]; then
    echo "error: CMake 3.13.5 is required for the GCC 4.8.5 validation lane" >&2
    exit 2
fi

mkdir -p "$RUN_ROOT"

echo "[gcc48] Debug C++11 and consumer/package matrix"
cmake -S . -B "$RUN_ROOT/build-debug" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_COMPILER=g++ \
    -DGINT_BUILD_TESTS=ON \
    -DGINT_BUILD_TESTS_RELEASE=OFF \
    -DGINT_BUILD_BENCHMARKS=OFF
cmake --build "$RUN_ROOT/build-debug" --parallel "$JOBS"
(cd "$RUN_ROOT/build-debug" && ctest --output-on-failure -j "$JOBS")

echo "[gcc48] Release/O3 C++11 matrix"
cmake -S . -B "$RUN_ROOT/build-release" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=g++ \
    -DGINT_BUILD_TESTS=ON \
    -DGINT_BUILD_TESTS_RELEASE=ON \
    -DGINT_BUILD_BENCHMARKS=OFF
cmake --build "$RUN_ROOT/build-release" --target gint_tests_release --parallel "$JOBS"
(cd "$RUN_ROOT/build-release" && ctest --output-on-failure -j "$JOBS" -R '^release\.')

echo "[gcc48] Independent differential oracle"
CXX=g++ \
GINT_DIFFERENTIAL_BUILD_DIR="$RUN_ROOT/differential" \
GINT_DIFFERENTIAL_ITERATIONS="$DIFFERENTIAL_ITERATIONS" \
    scripts/run-differential.sh

echo "[gcc48] Validation completed"
