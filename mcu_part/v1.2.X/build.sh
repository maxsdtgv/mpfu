#!/usr/bin/env bash
# Standalone build for the MPFU bootloader (PIC16F1789) using XC8 CLI.
# Useful for CI / quick size checks outside MPLAB X.
#
# Usage:  ./build.sh            # build + print memory summary
#         XC8=/path/to/bin ./build.sh
set -euo pipefail

XC8_BIN="${XC8:-/opt/microchip/xc8/v2.50/bin}"
CC="$XC8_BIN/xc8-cc"
MCPU=16f1789
# -O1 is available in XC8 Free mode and saves ~107 words vs -O0.
# -O2/-Os require a Pro/eval license.
OPT="${OPT:--O1}"
SRC_DIR="$(cd "$(dirname "$0")" && pwd)"
OUT_DIR="${OUT_DIR:-/tmp/mpfu_build}"

mkdir -p "$OUT_DIR"

SOURCES=(
    main.c
    bootloader.c
    internal_flash.c
    serial.c
    eeprom_25lc512.c
    mcc_generated_files/mcc.c
    mcc_generated_files/device_config.c
    mcc_generated_files/pin_manager.c
    mcc_generated_files/eusart.c
    mcc_generated_files/spi.c
)

cd "$SRC_DIR"
"$CC" -mcpu="$MCPU" "$OPT" -o "$OUT_DIR/mpfu.elf" "${SOURCES[@]}" \
    -Imcc_generated_files -I.
