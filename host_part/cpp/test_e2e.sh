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

# 2) mock MCU attached to the MCU side.
#    Preload a known bootloader trampoline at 0x0000-0x0003 so we can verify it
#    SURVIVES application flashing (WriteAppBlock must preserve words 0-3).
python3 - "$TTY_MCU" <<'PY' &
import os, sys
sys.path.insert(0, ".")
from mock_mcu import MockMCU
m = MockMCU(log=lambda *a: print(*a, file=sys.stderr))
# Known "GOTO bootloader" trampoline (same shape as a real one).
m.flash[0x0000] = 0x3180   # MOVLP 0
m.flash[0x0001] = 0x2802   # GOTO 0x002
m.flash[0x0002] = 0x31BF   # MOVLP 0x3F
m.flash[0x0003] = 0x2FB6   # GOTO bootloader
fd = os.open(sys.argv[1], os.O_RDWR | os.O_NOCTTY)
m.serve(fd, fd)
PY
MCU_PID=$!
sleep 1

RC=0

echo "=================================================================="
echo " TEST 1: device detection"
echo "=================================================================="
./mpfu -D "$TTY_HOST" -b 115200 -c 16f1789 -s 2>&1 | tee /tmp/mpfu_t1.log
grep -q "Device ID" /tmp/mpfu_t1.log && echo "PASS: device detected" || { echo "FAIL: device not detected"; RC=1; }

echo
echo "=================================================================="
echo " TEST 2: flash the fixture app (image v2)"
echo "=================================================================="
# blink_noirq is the fixture that used to trigger the reset-vector loop; it is
# the strongest test of the host's vector-chain resolution + app-vector block.
FW="../../test/blink_noirq/main.hex"
[ -f "$FW" ] || FW="../../test/blink_irq/main.hex"
echo "Using firmware: $FW"
if [ -f "$FW" ]; then
    timeout 60 ./mpfu -D "$TTY_HOST" -b 115200 -c 16f1789 -f "$FW" -s > /tmp/mpfu_t2.log 2>&1
    if grep -q "Flasing done" /tmp/mpfu_t2.log; then
        echo "PASS: flashing completed"
    else
        echo "FAIL: flashing did not complete"; tail -20 /tmp/mpfu_t2.log; RC=1
    fi
    if grep -q "Write to addr 0x3FE0" /tmp/mpfu_t2.log; then
        echo "PASS: app-vector row 0x3FE0 was written"
    else
        echo "FAIL: app-vector row 0x3FE0 not written"; RC=1
    fi
else
    echo "SKIP: no fixture hex present"
fi

echo
echo "=================================================================="
echo " TEST 2b: bootloader trampoline at 0x0000 survived flashing"
echo "=================================================================="
# Read back word 0x0000; must still equal the preloaded 0x3180 (MOVLP 0),
# i.e. the app's own reset vector must NOT have overwritten it.
timeout 30 ./mpfu -D "$TTY_HOST" -b 115200 -c 16f1789 -r /tmp/mpfu_rb2.txt > /tmp/mpfu_t2b.log 2>&1
V0000=$(grep -iE "^0000 " /tmp/mpfu_rb2.txt | awk '{print $2}' | cut -c1-4)
if [ "${V0000^^}" = "3180" ]; then
    echo "PASS: word 0x0000 preserved (0x$V0000)"
else
    echo "FAIL: word 0x0000 = 0x${V0000:-????}, expected 0x3180 (BL vector clobbered)"; RC=1
fi

echo
echo "=================================================================="
echo " TEST 3: read firmware back"
echo "=================================================================="
timeout 90 ./mpfu -D "$TTY_HOST" -b 115200 -c 16f1789 -r /tmp/mpfu_readback.txt > /tmp/mpfu_t3.log 2>&1
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
