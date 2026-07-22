#!/usr/bin/env bash
# Flash the MPFU bootloader to a PIC16F1789 using a PICkit 3 via ipecmd.
#
# Requires MPLAB X v5.35 (last version with PICkit 3 support) installed just
# for its ipecmd. XC8 builds the firmware separately (see build.sh).
#
# Usage:
#   ./flash.sh                 # build with build.sh, then flash the result
#   ./flash.sh path/to/fw.hex  # flash an existing hex
#   VERIFY_ONLY=1 ./flash.sh   # verify device connection only, do not program
#
# Env overrides:
#   IPE   - path to mplab_ipe dir (default: MPLAB X v5.35 location)
#   TOOL  - programmer tool id for ipecmd -T (default: PPK3 = PICkit 3)
#   DEV   - device id for ipecmd -P (default: 16F1789)
set -euo pipefail

IPE_DIR="${IPE:-/opt/microchip/mplabx/v5.35/mplab_platform/mplab_ipe}"
IPECMD="$IPE_DIR/ipecmd.sh"
TOOL="${TOOL:-PPK3}"
DEV="${DEV:-16F1789}"
SRC_DIR="$(cd "$(dirname "$0")" && pwd)"
OUT_DIR="${OUT_DIR:-/tmp/mpfu_build}"

[ -x "$IPECMD" ] || { echo "ERROR: ipecmd not found at $IPECMD"; echo "Install MPLAB X v5.35 or set IPE=..."; exit 1; }

# Connection check only
if [ "${VERIFY_ONLY:-0}" = 1 ]; then
    echo "== Checking programmer + target (no programming) =="
    cd "$IPE_DIR"
    exec "$IPECMD" -T"$TOOL" -P"$DEV" -OK
fi

# Determine hex to flash
if [ $# -ge 1 ]; then
    HEX="$1"
else
    echo "== Building firmware via build.sh =="
    "$SRC_DIR/build.sh" 2>&1 | grep -E "Program space|Data space|error|Error" || true
    HEX="$OUT_DIR/mpfu.hex"
fi

[ -f "$HEX" ] || { echo "ERROR: hex file not found: $HEX"; exit 1; }
echo "== Flashing $HEX to $DEV via $TOOL =="

# ipecmd flags: -M program entire device, -F<file>, -OL release from reset after
cd "$IPE_DIR"
"$IPECMD" -T"$TOOL" -P"$DEV" -M -F"$HEX" -OL
