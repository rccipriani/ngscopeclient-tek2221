#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
if [[ "${MSYSTEM:-}" != UCRT64 ]]; then echo 'Use MSYS2 UCRT64, matching your ngscopeclient compiler.' >&2; exit 1; fi
if [[ $# -lt 1 || $# -gt 2 ]]; then echo 'Usage: bash plugin/Build-Plugin.sh /c/path/to/scopehal-apps-stock [build-directory]' >&2; exit 1; fi
source_dir=$(cd -- "$1" && pwd)
host_build=${2:-"$source_dir/build"}
host_build=$(cd -- "$host_build" && pwd)
# Build only our external DLL. Do not run Apply-Driver.py or alter host sources.
cmake -S plugin -B plugin/build -G Ninja \
    -DNGSCOPECLIENT_SOURCE="$source_dir" -DNGSCOPECLIENT_BUILD="$host_build"
cmake --build plugin/build -j4
printf 'Built: %s/plugin/build/Tek2221Native.dll\n' "$PWD"
printf 'Host sources were not edited. Close the host before installing the DLL.\n'
