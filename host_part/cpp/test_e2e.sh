#!/usr/bin/env bash
# End-to-end local test for the MPFU host tool against a mock MCU.
#
# Creates a virtual serial pair with socat, runs mock_mcu.py on one side and
# the compiled ./mpfu on the other, then exercises device detection, flashing
# and reading. No real PIC hardware required.
set -uo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"

command -v socat >/dev/null || { echo "socat is required"; exit 1; }
[ -x ./mpfu ] || { echo "Building host tool..."; make >/dev/null || exit 1; }

TTY_HOST=/tmp/mpfu_tty_host
TTY_MCU=/tmp/mpfu_tty_mcu
rm -f "$TTY_HOST" "$TTY_MCU"

# 1) virtual serial pair
socat -d -d pty,raw,echo=0,link="$TTY_HOST" pty,raw,echo=0,link="$TTY_MCU" >/tmp/mpfu_socat.log 2>&1 &
SOCAT_PID=$!
sleep 1

cleanup(){ kill "$SOCAT_PID" "$MCU_PID" 2>/dev/null; }
trap cleanup EXIT

if [ ! -e "$TTY_HOST" ] || [ ! -e "$TTY_MCU" ]; then
    echo "FAIL: socat did not create ptys"; cat /tmp/mpfu_socat.log; exit 1
fi

# 2) mock MCU attached to the MCU side
python3 - "$TTY_MCU" <<'PY' &
import os, sys
sys.path.insert(0, ".")
from mock_mcu import MockMCU
fd = os.open(sys.argv[1], os.O_RDWR | os.O_NOCTTY)
MockMCU(log=lambda *a: print(*a, file=sys.stderr)).serve(fd, fd)
PY
MCU_PID=$!
sleep 1

RC=0

echo "=================================================================="
echo " TEST 1: device detection"
echo "=================================================================="
./mpfu -D "$TTY_HOST" -b 115200 -s 2>&1 | tee /tmp/mpfu_t1.log
grep -q "Device ID" /tmp/mpfu_t1.log && echo "PASS: device detected" || { echo "FAIL: device not detected"; RC=1; }

echo
echo "=================================================================="
echo " TEST 2: flash led.hex"
echo "=================================================================="
if [ -f led.hex ]; then
    timeout 60 ./mpfu -D "$TTY_HOST" -b 115200 -f led.hex -s > /tmp/mpfu_t2.log 2>&1
    if grep -q "Flasing done" /tmp/mpfu_t2.log; then
        echo "PASS: flashing completed"
    else
        echo "FAIL: flashing did not complete"; tail -20 /tmp/mpfu_t2.log; RC=1
    fi
else
    echo "SKIP: led.hex not present"
fi

echo
echo "=================================================================="
echo " TEST 3: read firmware back"
echo "=================================================================="
timeout 90 ./mpfu -D "$TTY_HOST" -b 115200 -r /tmp/mpfu_readback.txt > /tmp/mpfu_t3.log 2>&1
if grep -q "Reading done" /tmp/mpfu_t3.log && [ -s /tmp/mpfu_readback.txt ]; then
    echo "PASS: readback produced $(wc -l < /tmp/mpfu_readback.txt) lines"
else
    echo "FAIL: readback failed"; tail -20 /tmp/mpfu_t3.log; RC=1
fi

echo
echo "=================================================================="
[ "$RC" = 0 ] && echo " ALL TESTS PASSED" || echo " SOME TESTS FAILED"
echo "=================================================================="
exit $RC
