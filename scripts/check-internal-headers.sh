#!/usr/bin/env bash

set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cxx_command=${1:-${CXX:-c++}}

# Preserve an explicitly named executable whose path contains spaces. Otherwise
# split the command on whitespace so common launcher forms such as
# CXX='ccache c++' and CXX='env c++' work on the Bash 3 shipped by macOS.
cxx=()
if [[ -x "$cxx_command" ]]; then
    cxx=("$cxx_command")
else
    read -r -a cxx <<< "$cxx_command"
fi
if [[ ${#cxx[@]} -eq 0 ]]; then
    echo "error: CXX command must not be empty" >&2
    exit 2
fi

headers=()
while IFS= read -r -d '' header; do
    headers[${#headers[@]}]=$header
done < <(find "$root/src/gint" -type f -name '*.hpp' -print0)

if [[ ${#headers[@]} -eq 0 ]]; then
    echo "error: no internal headers found under $root/src/gint" >&2
    exit 1
fi

for header in "${headers[@]}"; do
    echo "checking ${header#"$root"/}"
    "${cxx[@]}" \
        -std=c++11 \
        -Wall \
        -Wextra \
        -Werror \
        -fsyntax-only \
        -include "$header" \
        "$root/tests/consumer/internal_header_smoke.cpp"
done
