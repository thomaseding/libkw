#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"

CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"
RUFF="${RUFF:-ruff}"

if ! command -v "$CLANG_FORMAT" >/dev/null 2>&1; then
    echo "error: clang-format not found on PATH" >&2
    exit 1
fi

if ! command -v "$RUFF" >/dev/null 2>&1; then
    echo "error: ruff not found on PATH" >&2
    exit 1
fi

mapfile -d '' files < <(find src test -type f \( -name '*.hpp' -o -name '*.cpp' \) -print0)
mapfile -d '' py_files < <(find src test -type f -name '*.py' -print0)

"$CLANG_FORMAT" -i "${files[@]}"
"$RUFF" format "${py_files[@]}"
