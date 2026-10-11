#!/usr/bin/env bash
# Use an existing configured server build for include paths and libluabind.
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
build=$(cd "${1:?Usage: run.sh /path/to/server/build}" && pwd)
flags="$build/zone/CMakeFiles/zone.dir/flags.make"
includes=$(sed -n 's/^CXX_INCLUDES = //p' "$flags")
work=$(mktemp -d)
trap 'rm -f "$work/test.cpp" "$work/test"; rmdir "$work"' EXIT
python3 "$root/tests/lua_create_npc/generate_test.py" "$work/test.cpp"
# The generated Makefile include list is intentionally expanded as compiler args.
${CXX:-c++} -std=c++20 -g -O1 -fsanitize=address -fno-omit-frame-pointer \
    -I"$root" $includes "$work/test.cpp" "$build/bin/libluabind.a" \
    -llua5.1 -ldl -o "$work/test"
ASAN_OPTIONS=detect_leaks=1 "$work/test"
