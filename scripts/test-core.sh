#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/core
sources=()
while IFS= read -r source || [[ -n "$source" ]]; do
  case "$source" in
    ""|\#*) continue ;;
  esac
  sources+=("$source")
done < cmake/core-sources.txt
if [[ ${#sources[@]} -eq 0 ]]; then
  echo "No core sources listed in cmake/core-sources.txt" >&2
  exit 1
fi
"${CXX:-g++}" -std=c++17 -O2 -Wall -Wextra -Wpedantic \
  "${sources[@]}" \
  tests/CoreTests.cpp -o build/core/arranger-tests
build/core/arranger-tests build/core
build/core/arranger-tests --arrangement-review build/core
build/core/arranger-tests --harmony-recheck
