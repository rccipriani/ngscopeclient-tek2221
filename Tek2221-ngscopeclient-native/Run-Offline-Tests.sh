#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
test_dir=$(mktemp -d)
trap 'rm -rf -- "$test_dir"' EXIT
for name in NativeTests ExtendedTests RecentConnectionTests StatusTests ResourceTests; do
    g++ -std=c++17 -O2 -Wall -Wextra -Werror -Idriver -Igui "tests/$name.cpp" driver/Tek2221Native.cpp driver/Tek2221Controls.cpp -o "$test_dir/$name"
    "$test_dir/$name" fixtures
done
