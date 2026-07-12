#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: scripts/bootstrap-validation-env.sh [options]

Prepare the current validation environment under runs/<scope>/ without running
project tests or benchmarks.

Options:
  --scope <name>              Output scope under runs/ (default: linux)
  --compiler <cxx>            C++ compiler to prepare; repeatable
                              (default: g++ and clang++ when available)
  --benchmark-version <ver>   Google Benchmark version, with or without v
                              prefix (default: 1.9.5)
  --benchmark-sha256 <hash>   Expected source archive SHA-256. Required for
                              versions other than the pinned default.
  --skip-packages             Do not install OS packages
  --jobs <n>                  Build parallelism (default: detected CPU count)
  -h, --help                  Show this help

Generated files:
  runs/<scope>/env/<compiler-path-id>.env

The generated env files set CMAKE_CXX_COMPILER and prepend the matching
Release-built Google Benchmark prefix to CMAKE_PREFIX_PATH. Callers choose
their own CMake targets, make targets, BENCH_ARGS, filters, and output files.
EOF
}

scope="linux"
benchmark_version="1.9.5"
benchmark_sha256=""
skip_packages=0
jobs=""
declare -a requested_compilers=()

while (($#)); do
    case "$1" in
        --scope)
            scope="${2:?missing value for --scope}"
            shift 2
            ;;
        --compiler)
            requested_compilers+=("${2:?missing value for --compiler}")
            shift 2
            ;;
        --benchmark-version)
            benchmark_version="${2:?missing value for --benchmark-version}"
            shift 2
            ;;
        --benchmark-sha256)
            benchmark_sha256="${2:?missing value for --benchmark-sha256}"
            shift 2
            ;;
        --skip-packages)
            skip_packages=1
            shift
            ;;
        --jobs)
            jobs="${2:?missing value for --jobs}"
            shift 2
            ;;
        -h | --help)
            usage
            exit 0
            ;;
        *)
            echo "error: unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if [[ "$scope" == /* || "$scope" == *"/"* || "$scope" == *".."* ]]; then
    echo "error: --scope must be a simple runs/ scope name" >&2
    exit 2
fi

if [[ -z "$jobs" ]]; then
    if command -v nproc >/dev/null 2>&1; then
        jobs="$(nproc)"
    elif command -v getconf >/dev/null 2>&1; then
        jobs="$(getconf _NPROCESSORS_ONLN)"
    else
        jobs="1"
    fi
fi

version="${benchmark_version#v}"
tag="v${version}"
if [[ -z "$benchmark_sha256" && "$version" == "1.9.5" ]]; then
    benchmark_sha256="9631341c82bac4a288bef951f8b26b41f69021794184ece969f8473977eaa340"
fi
if [[ ! "$benchmark_sha256" =~ ^[0-9a-fA-F]{64}$ ]]; then
    echo "error: --benchmark-sha256 is required and must contain 64 hexadecimal digits" >&2
    exit 2
fi
run_root="runs/${scope}"
deps_root="${run_root}/deps"
src_root="${deps_root}/src"
env_root="${run_root}/env"

mkdir -p "$src_root" "$env_root"

apt_get() {
    if [[ "${EUID:-$(id -u)}" -eq 0 ]]; then
        env DEBIAN_FRONTEND=noninteractive apt-get "$@"
        return
    fi

    if command -v sudo >/dev/null 2>&1; then
        sudo env DEBIAN_FRONTEND=noninteractive apt-get "$@"
        return
    fi

    echo "error: package installation needs root or sudo; use --skip-packages to skip it" >&2
    exit 2
}

install_packages() {
    if ((skip_packages)); then
        echo "[env] skipping package installation"
        return
    fi

    if command -v apt-get >/dev/null 2>&1; then
        echo "[env] installing Ubuntu/Debian validation packages"
        apt_get update
        apt_get install -y \
            build-essential \
            ca-certificates \
            clang \
            cmake \
            curl \
            git \
            libboost-dev \
            libfmt-dev \
            libgtest-dev \
            ninja-build \
            pkg-config \
            rsync \
            tar
        return
    fi

    echo "[env] apt-get not found; assuming compiler, CMake, fmt, GTest, and Boost are already installed"
}

resolve_compilers() {
    local -a input=("$@")
    local -a resolved=()
    local candidate path

    if ((${#input[@]} == 0)); then
        input=(g++ clang++)
    fi

    for candidate in "${input[@]}"; do
        if [[ "$candidate" == */* && -x "$candidate" ]]; then
            path="$(cd "$(dirname "$candidate")" && pwd -P)/$(basename "$candidate")"
            resolved+=("$path")
        elif path="$(command -v "$candidate" 2>/dev/null)"; then
            resolved+=("$path")
        elif ((${#requested_compilers[@]} > 0)); then
            echo "error: compiler not found: $candidate" >&2
            exit 2
        else
            echo "[env] compiler not found, skipping: $candidate" >&2
        fi
    done

    if ((${#resolved[@]} == 0)); then
        echo "error: no C++ compilers found" >&2
        exit 2
    fi

    printf '%s\n' "${resolved[@]}"
}

compiler_id() {
    local id path
    path="$1"
    if [[ "$path" != /* ]]; then
        path="$(command -v "$path")"
    fi

    id="${path#/}"
    id="${id//++/xx}"
    id="${id//\//_}"
    id="${id//[^A-Za-z0-9._-]/_}"
    printf '%s\n' "$id"
}

shell_quote() {
    printf '%q' "$1"
}

download_benchmark_archive() {
    local archive="$1"
    local tmp_archive="${archive}.tmp.$$"

    rm -f "$tmp_archive"
    if ! curl --fail --location --retry 3 --retry-delay 2 --connect-timeout 20 --max-time 300 \
        "https://github.com/google/benchmark/archive/refs/tags/${tag}.tar.gz" -o "$tmp_archive"; then
        rm -f "$tmp_archive"
        echo "error: failed to download Google Benchmark ${tag}" >&2
        return 1
    fi

    mv "$tmp_archive" "$archive"
}

verify_benchmark_archive() {
    local archive="$1"
    local actual expected

    if command -v sha256sum >/dev/null 2>&1; then
        actual="$(sha256sum "$archive")"
    else
        actual="$(shasum -a 256 "$archive")"
    fi
    actual="${actual%% *}"
    actual="$(printf '%s' "$actual" | tr '[:upper:]' '[:lower:]')"
    expected="$(printf '%s' "$benchmark_sha256" | tr '[:upper:]' '[:lower:]')"
    if [[ "$actual" != "$expected" ]]; then
        echo "error: Google Benchmark ${tag} archive checksum mismatch" >&2
        echo "error: expected ${benchmark_sha256}, got ${actual}" >&2
        return 1
    fi
}

fetch_benchmark() {
    local archive="${src_root}/benchmark-${version}.tar.gz"
    local src_dir="${src_root}/benchmark-${version}"

    if [[ -f "$archive" ]] && ! verify_benchmark_archive "$archive"; then
        rm -f "$archive"
    fi
    if [[ ! -f "$archive" ]]; then
        echo "[env] downloading Google Benchmark ${tag}" >&2
        download_benchmark_archive "$archive" || return 1
    fi
    if ! verify_benchmark_archive "$archive"; then
        rm -f "$archive"
        return 1
    fi

    rm -rf "$src_dir"
    if ! tar -xzf "$archive" -C "$src_root"; then
        rm -rf "$src_dir"
        rm -f "$archive"
        echo "error: failed to extract Google Benchmark archive; removed ${archive}" >&2
        return 1
    fi
    if [[ ! -d "$src_dir" ]]; then
        echo "error: Google Benchmark archive did not extract to ${src_dir}" >&2
        rm -f "$archive"
        return 1
    fi

    printf '%s\n' "$src_dir"
}

build_benchmark_for_compiler() {
    local compiler="$1"
    local src_dir="$2"
    local id build_dir install_dir env_file

    id="$(compiler_id "$compiler")"
    build_dir="${deps_root}/google-benchmark-${version}/${id}/build"
    install_dir="${deps_root}/google-benchmark-${version}/${id}/install"
    env_file="${env_root}/${id}.env"

    echo "[env] building Google Benchmark ${tag} for ${compiler} -> ${install_dir}"
    cmake -S "$src_dir" -B "$build_dir" \
        -DCMAKE_CXX_COMPILER="$compiler" \
        -DCMAKE_INSTALL_PREFIX="$PWD/$install_dir" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBENCHMARK_ENABLE_TESTING=OFF \
        -DBENCHMARK_ENABLE_GTEST_TESTS=OFF \
        -DBENCHMARK_ENABLE_LIBPFM=OFF
    cmake --build "$build_dir" --parallel "$jobs"
    cmake --build "$build_dir" --target install --parallel "$jobs"

    {
        echo "# Generated by scripts/bootstrap-validation-env.sh"
        echo "# Source this before configuring a build with this compiler."
        printf 'export GINT_VALIDATION_SCOPE=%s\n' "$(shell_quote "$scope")"
        printf 'export GINT_COMPILER_ID=%s\n' "$(shell_quote "$id")"
        printf 'export CMAKE_CXX_COMPILER=%s\n' "$(shell_quote "$compiler")"
        printf 'export CXX=%s\n' "$(shell_quote "$compiler")"
        printf 'export GINT_BENCHMARK_PREFIX=%s\n' "$(shell_quote "$PWD/$install_dir")"
        cat <<'EOF'
if [ -n "${CMAKE_PREFIX_PATH:-}" ]; then
    export CMAKE_PREFIX_PATH="${GINT_BENCHMARK_PREFIX}:${CMAKE_PREFIX_PATH}"
else
    export CMAKE_PREFIX_PATH="${GINT_BENCHMARK_PREFIX}"
fi
EOF
    } >"$env_file"

    echo "[env] wrote ${env_file}"
}

install_packages
compilers=()
compiler_output=""
if ! compiler_output="$(resolve_compilers "${requested_compilers[@]}")"; then
    echo "error: failed to resolve requested compilers" >&2
    exit 2
fi
while IFS= read -r compiler_path; do
    [[ -n "$compiler_path" ]] || continue
    compilers+=("$compiler_path")
done <<<"$compiler_output"
if ! benchmark_src="$(fetch_benchmark)"; then
    echo "error: failed to prepare Google Benchmark ${tag}" >&2
    exit 2
fi

for compiler in "${compilers[@]}"; do
    build_benchmark_for_compiler "$compiler" "$benchmark_src"
done

cat <<EOF
[env] done

Next step examples (choose your own targets and BENCH_ARGS):
  source ${env_root}/$(compiler_id "${compilers[0]}").env
  cmake -S . -B ${run_root}/build-\${GINT_COMPILER_ID}-<name> -DGINT_BUILD_TESTS=ON -DGINT_BUILD_BENCHMARKS=OFF -DCMAKE_CXX_COMPILER="\$CMAKE_CXX_COMPILER"
  cmake -S . -B ${run_root}/build-bench-\${GINT_COMPILER_ID}-<name> -DGINT_BUILD_TESTS=OFF -DGINT_BUILD_BENCHMARKS=ON -DCMAKE_PREFIX_PATH="\$CMAKE_PREFIX_PATH" -DCMAKE_CXX_COMPILER="\$CMAKE_CXX_COMPILER"
EOF
