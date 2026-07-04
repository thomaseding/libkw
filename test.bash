#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"

GXX="${GXX:-g++}"

if ! command -v "$GXX" >/dev/null 2>&1; then
    echo "error: g++ not found on PATH (required by test/modal.py)" >&2
    exit 1
fi
export GXX

BUILD_DIR="build"

# Configure once; re-run manually (or delete build/) after CMakeLists.txt changes.
if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ]; then
    cmake -S . -B "${BUILD_DIR}" -G Ninja
fi

# Build and run the basic test executable.
cmake --build "${BUILD_DIR}" --target test_basic
./"${BUILD_DIR}"/test_basic

# libkw.hpp must be fully self-contained (no reliance on a transitive
# #include arriving via some other header included first in a real program).
cmake --build "${BUILD_DIR}" --target test_standalone_include
./"${BUILD_DIR}"/test_standalone_include

# Compile-fail cases: each test/expect_failure/*.cpp must fail to build with
# output matching its .oracle file.
python3 test/expect_failure.py

# modal.cpp is compiled/run by this script itself (many -D combos via g++),
# not through the test_modal CMake target.
python3 test/modal.py

# TODO: run clang-tidy here once it can parse P2996 reflection syntax
# (std::meta::, ^^, splicing). As of clang 22.1.6, clang-tidy hard-errors
# on this code (unknown -freflection flag, unparsable ^^/[: :] syntax).
