#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
if [[ $# -ne 1 ]]; then echo 'Usage: bash plugin/Install-Plugin.sh /c/path/to/host-build/src/ngscopeclient' >&2; exit 1; fi
host_dir=$(cd -- "$1" && pwd)
[[ -f "$host_dir/ngscopeclient.exe" && -f plugin/build/Tek2221Native.dll ]] || { echo 'Host EXE or built plugin DLL missing.' >&2; exit 1; }
mkdir -p -- "$host_dir/plugins"
if [[ -f "$host_dir/plugins/Tek2221Native.dll" ]]; then cp -p -- "$host_dir/plugins/Tek2221Native.dll" "$host_dir/plugins/Tek2221Native.dll.bak"; fi
cp -- plugin/build/Tek2221Native.dll "$host_dir/plugins/Tek2221Native.dll"
printf 'Installed DLL only: %s/plugins/Tek2221Native.dll\n' "$host_dir"
printf 'Launch with: bash plugin/Run-Stock.sh "%s"\n' "$host_dir"
