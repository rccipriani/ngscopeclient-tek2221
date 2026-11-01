#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
if [[ "${MSYSTEM:-}" != UCRT64 ]]; then
    echo 'Use the MSYS2 UCRT64 shell for this 64-bit Windows build.' >&2
    exit 1
fi
g++ -std=c++17 -O2 -Wall -Wextra -Idriver \
    native/VisaCheck.cpp driver/Tek2221Native.cpp driver/Tek2221VISA.cpp \
    -o Tek2221VisaCheck.exe
echo 'Built Tek2221VisaCheck.exe (64 bit). First run: ./Tek2221VisaCheck.exe --load-only'
