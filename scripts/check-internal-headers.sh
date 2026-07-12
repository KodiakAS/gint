#!/usr/bin/env bash

set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cxx=${1:-${CXX:-c++}}

for header in "$root"/src/gint/*.hpp; do
    echo "checking ${header#"$root"/}"
    "$cxx" \
        -std=c++11 \
        -Wall \
        -Wextra \
        -Werror \
        -fsyntax-only \
        -include "$header" \
        "$root/tests/consumer/internal_header_smoke.cpp"
done
