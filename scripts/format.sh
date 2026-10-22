#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
echo "$root"
cd "$root"
if ! command -v clang-format >/dev/null 2>&1; then
    echo "clang-format is required" >&2
    exit 1
fi
if [ "${1:-}" = "--check" ]; then
    find app/main_app/src tests -type f \( -name '*.c' -o -name '*.h' \) -exec clang-format --dry-run --Werror {} +
else
    find app/main_app/src tests -type f \( -name '*.c' -o -name '*.h' \) -exec clang-format -i {} +
fi
