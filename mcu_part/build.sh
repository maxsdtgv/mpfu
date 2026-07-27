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
# Extra compiler flags, e.g. feature switches or a shorter watchdog for testing:
#   EXTRA="-DUSE_DEVICE_ID_CHECK" ./build.sh
#   EXTRA="-DWDT_BL_TIMEOUT_CONF=0x1B" ./build.sh     # ~8 s dead-man
# Keep OPT to a single option; put anything additional in EXTRA (it is split on
# whitespace, so several flags work).
EXTRA="${EXTRA:-}"
read -r -a EXTRA_FLAGS <<< "$EXTRA"
# Place the bootloader at the END of flash and reserve the two top rows:
#   -0-37BF   : give 0x0000-0x37BF to the application
#   -3FC0-3FFF: reserve the flags row (0x3FC0) and the app-vector row (0x3FE0)
# The bootloader code then lands in 0x37C0-0x3FBF (2048 words). The reset vector at 0x0000 is
# still emitted by XC8 automatically (GOTO bootloader entry). See docs/MEMORY.md.
ROM="${ROM:-default,-0-37BF,-3FC0-3FFF}"
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
"$CC" -mcpu="$MCPU" "$OPT" ${EXTRA_FLAGS[@]+"${EXTRA_FLAGS[@]}"} -mrom="$ROM" -o "$OUT_DIR/mpfu.elf" "${SOURCES[@]}" \
    -Imcc_generated_files -I.
