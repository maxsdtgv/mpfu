#!/usr/bin/env bash
# Flash the MPFU bootloader to a PIC16F1789 using a PICkit 3 via ipecmd.
#
# Requires MPLAB X v5.35 (last version with PICkit 3 support) installed just
# for its ipecmd. XC8 builds the firmware separately (see build.sh).
#
# IMPORTANT: ipecmd v5.35 must run on its BUNDLED Java 8. The install did not
# patch ipecmd.sh's jdkhome, so that wrapper falls back to the system JVM
# (Java 21 here) and then fails with "Hex File could not be read". We therefore
# call ipecmd.jar directly with the bundled JRE 1.8.
#
# Usage:
#   ./flash.sh                 # build with build.sh, then flash the result
#   ./flash.sh path/to/fw.hex  # flash an existing hex
#   VERIFY_ONLY=1 ./flash.sh   # check programmer/target connection only
#   RELEASE_ONLY=1 ./flash.sh  # just release the device from reset (run app)
#
# Env overrides:
#   MPLABX - MPLAB X install dir (default: v5.35 location)
#   JAVA8  - path to Java 8 binary (default: bundled JRE in MPLABX)
#   TOOL   - programmer tool id for ipecmd -T (default: PPK3 = PICkit 3)
#   DEV    - device id for ipecmd -P (default: 16F1789)
set -euo pipefail

MPLABX="${MPLABX:-/opt/microchip/mplabx/v5.35}"
IPECMD_JAR="$MPLABX/mplab_platform/mplab_ipe/ipecmd.jar"
JAVA8="${JAVA8:-$MPLABX/sys/java/jre1.8.0_181/bin/java}"
TOOL="${TOOL:-PPK3}"
DEV="${DEV:-16F1789}"
SRC_DIR="$(cd "$(dirname "$0")" && pwd)"
OUT_DIR="${OUT_DIR:-/tmp/mpfu_build}"

[ -f "$IPECMD_JAR" ] || { echo "ERROR: ipecmd.jar not found at $IPECMD_JAR"; echo "Install MPLAB X v5.35 or set MPLABX=..."; exit 1; }
[ -x "$JAVA8" ]      || { echo "ERROR: bundled Java 8 not found at $JAVA8 (set JAVA8=...)"; exit 1; }

# ipecmd writes log files into the CWD, so run from a writable dir.
run_ipecmd() { ( cd /tmp && "$JAVA8" -jar "$IPECMD_JAR" "$@" ); }

# Connection check only
if [ "${VERIFY_ONLY:-0}" = 1 ]; then
    echo "== Checking programmer + target (no programming) =="
    run_ipecmd -T"$TOOL" -P"$DEV" -OK
    exit $?
fi

# Release from reset only (start whatever is on the chip)
if [ "${RELEASE_ONLY:-0}" = 1 ]; then
    echo "== Releasing device from reset =="
    run_ipecmd -T"$TOOL" -P"$DEV" -OL
    exit $?
fi

# Determine hex to flash
if [ $# -ge 1 ]; then
    HEX="$(readlink -f "$1")"
else
    echo "== Building firmware via build.sh =="
    "$SRC_DIR/build.sh" 2>&1 | grep -E "Program space|Data space|error|Error" || true
    HEX="$OUT_DIR/mpfu.hex"
fi

[ -f "$HEX" ] || { echo "ERROR: hex file not found: $HEX"; exit 1; }
echo "== Flashing $HEX to $DEV via $TOOL =="

# -M program entire device, -F<file>, -OL release from reset after programming
run_ipecmd -T"$TOOL" -P"$DEV" -M -F"$HEX" -OL
