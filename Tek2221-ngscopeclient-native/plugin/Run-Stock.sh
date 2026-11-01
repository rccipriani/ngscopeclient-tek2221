#!/usr/bin/env bash
set -euo pipefail
if [[ $# -ne 1 ]]; then echo 'Usage: bash plugin/Run-Stock.sh /c/path/to/host-build/src/ngscopeclient' >&2; exit 1; fi
# Stock Windows InitializePlugins finds plugins by EXE path, but LoadLibrary uses
# a relative plugins/ path. Launch from the EXE directory to satisfy both.
cd -- "$1"
exec ./ngscopeclient.exe
