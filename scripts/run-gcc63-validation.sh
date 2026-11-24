#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT_DIR"

RUN_ROOT=${GINT_GCC63_RUN_ROOT:-runs/ci/gcc63}
JOBS=${GINT_GCC63_JOBS:-2}
mkdir -p "$RUN_ROOT"
RUN_ROOT=$(cd "$RUN_ROOT" && pwd)
PREFIX="$RUN_ROOT/prefix"

export PATH="$RUN_ROOT/cmake/bin:$RUN_ROOT/python/bin:$PATH"
export PYTHONDONTWRITEBYTECODE=1
export TMPDIR="$RUN_ROOT/tmp"
mkdir -p "$TMPDIR"

if [[ "$(g++ -dumpversion)" != 6.3.0 ]]; then
    echo "error: GCC 6.3.0 is required, found $(g++ -dumpversion)" >&2
    exit 2
fi
cmake --version
python3 --version

dependency_options=(
    -DCMAKE_CXX_COMPILER=g++
    -DCMAKE_CXX_STANDARD=11
    -DCMAKE_CXX_STANDARD_REQUIRED=ON
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_INSTALL_PREFIX="$PREFIX"
    -DBUILD_SHARED_LIBS=OFF
)
cmake -S "$RUN_ROOT/fmt" -B "$RUN_ROOT/build-fmt" "${dependency_options[@]}" -DFMT_TEST=OFF -DFMT_DOC=OFF
cmake --build "$RUN_ROOT/build-fmt" --target install --parallel "$JOBS"
cmake -S "$RUN_ROOT/googletest" -B "$RUN_ROOT/build-googletest" "${dependency_options[@]}" -DBUILD_GMOCK=OFF
cmake --build "$RUN_ROOT/build-googletest" --target install --parallel "$JOBS"

test_options=(
    -DCMAKE_CXX_COMPILER=g++
    -DCMAKE_BUILD_TYPE=Debug
    -DCMAKE_PREFIX_PATH="$PREFIX"
    -DPython3_EXECUTABLE="$RUN_ROOT/python/bin/python3"
    -DGINT_BUILD_TESTS=ON
    -DGINT_BUILD_BENCHMARKS=OFF
)

echo "[gcc63] C++11 Debug, Release/O3 and consumer/package tests"
cmake -S . -B "$RUN_ROOT/build" "${test_options[@]}" -DGINT_BUILD_TESTS_RELEASE=ON
cmake --build "$RUN_ROOT/build" --parallel "$JOBS"
(cd "$RUN_ROOT/build" && ctest --output-on-failure -j "$JOBS")

echo "[gcc63] C++11 ASan/UBSan tests"
# CMake runs the test executables during GoogleTest discovery as well as in CTest.
export ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1
cmake -S . -B "$RUN_ROOT/build-sanitizers" "${test_options[@]}" \
    -DENABLE_SANITIZERS=ON -DGINT_BUILD_TESTS_RELEASE=OFF
cmake --build "$RUN_ROOT/build-sanitizers" --target gint_sanitized_tests --parallel "$JOBS"
(cd "$RUN_ROOT/build-sanitizers" && ctest --output-on-failure -L '^gint_sanitized$' -j "$JOBS")
