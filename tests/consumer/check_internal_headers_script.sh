#!/usr/bin/env bash

set -euo pipefail

checker=$1
work=$2
fixture="$work/fixture with spaces"
fake_bin="$work/bin"
log="$work/headers.log"

rm -rf "$work"
mkdir -p "$fixture/scripts" "$fixture/src/gint/nested directory" \
    "$fixture/tests/consumer" "$fake_bin"
cp "$checker" "$fixture/scripts/check-internal-headers.sh"
printf '#pragma once\n' > "$fixture/src/gint/root.hpp"
printf '#pragma once\n' > "$fixture/src/gint/nested directory/nested header.hpp"
printf 'int main() { return 0; }\n' > "$fixture/tests/consumer/internal_header_smoke.cpp"

fake_cxx="$fake_bin/fake-cxx"
# These variables must be expanded by the generated fake compiler, not here.
# shellcheck disable=SC2016
printf '%s\n' \
    '#!/usr/bin/env bash' \
    'set -euo pipefail' \
    'header=' \
    'while [[ $# -gt 0 ]]; do' \
    '    if [[ $1 == -include ]]; then' \
    '        header=$2' \
    '        shift 2' \
    '    else' \
    '        shift' \
    '    fi' \
    'done' \
    '[[ -n $header ]]' \
    'printf "%s\n" "$header" >> "$GINT_INTERNAL_HEADER_LOG"' \
    > "$fake_cxx"
chmod +x "$fake_cxx"

PATH="$fake_bin:$PATH" GINT_INTERNAL_HEADER_LOG="$log" \
    bash "$fixture/scripts/check-internal-headers.sh" 'env fake-cxx'

[[ $(wc -l < "$log") -eq 2 ]]
grep -Fqx "$fixture/src/gint/root.hpp" "$log"
grep -Fqx "$fixture/src/gint/nested directory/nested header.hpp" "$log"
