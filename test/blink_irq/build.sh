#!/usr/bin/env bash
# Build the blink_irq test-fixture application (PIC16F1789) with XC8.
# Produces main.hex — a NORMALLY compiled app (reset 0x0000, ISR 0x0004),
# used to test the mpfu host's row-0 / vector handling.
set -euo pipefail
XC8_BIN="${XC8:-/opt/microchip/xc8/v2.50/bin}"
DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"
"$XC8_BIN/xc8-cc" -mcpu=16f1789 -O1 -o main.elf main.c ../common/app_bootentry.c
echo "Built: $DIR/main.hex"
