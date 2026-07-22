#!/usr/bin/env python3
"""
Mock MCU for the MPFU bootloader protocol.

Speaks the same UART frame protocol as mcu_part/v1.2.X so the host tool
(host_part/cpp/mpfu) can be tested end-to-end without real PIC hardware.

Frame format (see docs/BL_protocol.txt):
    Host -> MCU:  0x55 LEN CMD DATA...      (LEN counts from LEN byte)
    MCU  -> Host: 0xAA LEN CMD DATA...

Run standalone for a self-test, or import and use MockMCU with a serial fd.
"""
import sys

PREAM_FROM_HOST = 0x55
PREAM_TO_HOST = 0xAA

READ_FROM_MEM = 0x02
WRITE_TO_MEM = 0x04
READ_FROM_SERIAL_EEPROM = 0x12
WRITE_TO_SERIAL_EEPROM = 0x14
START_APPLICATION = 0x0F

SUCCESS_CODE = 0xEE
ERROR_CODE = 0xFF

BLOCK_WORDS = 32           # 32 words per flash row
BLOCK_BYTES = BLOCK_WORDS * 2
FLASH_WORDS = 0x4000       # PIC16F1789 program memory
ERASED = 0x3FFF            # blank flash word (14-bit)


class MockMCU:
    def __init__(self, log=lambda *a: None):
        self.flash = [ERASED] * FLASH_WORDS
        self.config = {0x8005: 0x3120, 0x8006: 0x3180}  # rev / device id (arbitrary)
        self.eeprom = bytearray(b"\xFF" * 0x10000)
        self.started = False
        self.log = log

    # --- flash access -----------------------------------------------------
    def read_word(self, addr):
        if addr & 0x8000:
            return self.config.get(addr, ERASED)
        return self.flash[addr] & 0x3FFF if addr < FLASH_WORDS else ERASED

    def write_block(self, addr, words):
        if addr & 0x8000:
            self.config[addr] = words[0]
            return
        for i, w in enumerate(words):
            if addr + i < FLASH_WORDS:
                self.flash[addr + i] = w & 0x3FFF

    # --- frame handling ---------------------------------------------------
    def handle_frame(self, length, cmd, data):
        """data = bytes after CMD. Returns response frame bytes (incl 0xAA) or b''."""
        if cmd == READ_FROM_MEM:
            addr = (data[0] << 8) | data[1]
            if data[0] == 0x80:                      # config space: 1 word
                w = self.read_word(addr)
                payload = bytes([(w >> 8) & 0xFF, w & 0xFF])
            else:                                    # 32 words, big-endian
                payload = bytearray()
                for i in range(BLOCK_WORDS):
                    w = self.read_word(addr + i)
                    payload += bytes([(w >> 8) & 0xFF, w & 0xFF])
            resp = bytes([len(payload) + 2, READ_FROM_MEM]) + payload
            return bytes([PREAM_TO_HOST]) + resp

        if cmd == WRITE_TO_MEM:
            addr = (data[0] << 8) | data[1]
            body = data[2:]
            if data[0] == 0x80:
                self.write_block(addr, [(body[0] << 8) | body[1]])
            else:
                words = []
                for i in range(0, BLOCK_BYTES, 2):
                    words.append((body[i] << 8) | body[i + 1])
                self.write_block(addr, words)
            return bytes([PREAM_TO_HOST, 0x02, SUCCESS_CODE])

        if cmd == START_APPLICATION:
            self.started = True
            self.log("[MOCK] START_APPLICATION received")
            return b""                               # host does not wait

        # Unknown command -> error, mirrors DefineError()
        return bytes([PREAM_TO_HOST, 0x02, ERROR_CODE])

    def serve(self, read_fd, write_fd):
        """Blocking loop: read frames from read_fd, write responses to write_fd."""
        import os
        buf = bytearray()
        while True:
            try:
                chunk = os.read(read_fd, 256)
            except OSError:
                break
            if not chunk:
                continue
            buf += chunk
            # parse as many complete frames as available
            while True:
                # find preamble
                while buf and buf[0] != PREAM_FROM_HOST:
                    buf.pop(0)
                if len(buf) < 2:
                    break
                length = buf[1]                      # bytes counting from this byte
                total = 1 + length                   # + preamble
                if len(buf) < total:
                    break
                frame = buf[:total]
                del buf[:total]
                cmd = frame[2]
                data = frame[3:]
                resp = self.handle_frame(length, cmd, bytes(data))
                if resp:
                    os.write(write_fd, resp)


def _selftest():
    """Exercise the protocol logic without any serial port."""
    m = MockMCU()
    ok = True

    # write a 32-word block at 0x0080, then read it back
    addr = 0x0080
    payload = bytearray()
    for i in range(BLOCK_WORDS):
        w = 0x0100 + i
        payload += bytes([(w >> 8) & 0xFF, w & 0xFF])
    frame = bytes([0x44, WRITE_TO_MEM, (addr >> 8) & 0xFF, addr & 0xFF]) + payload
    r = m.handle_frame(frame[1], frame[1], frame[2:])
    assert r[2] == SUCCESS_CODE, "write should ACK"

    rd = m.handle_frame(0x04, READ_FROM_MEM, bytes([(addr >> 8) & 0xFF, addr & 0xFF]))
    # rd = AA LEN CMD <64 bytes>
    got = []
    for i in range(BLOCK_WORDS):
        hi = rd[3 + i * 2]
        lo = rd[4 + i * 2]
        got.append((hi << 8) | lo)
    expected = [0x0100 + i for i in range(BLOCK_WORDS)]
    if got != expected:
        print("FAIL: readback mismatch")
        print("  expected:", [hex(x) for x in expected[:4]], "...")
        print("  got     :", [hex(x) for x in got[:4]], "...")
        ok = False
    else:
        print("PASS: write+readback of 32-word block at 0x%04X" % addr)

    # config read (device id)
    rd = m.handle_frame(0x04, READ_FROM_MEM, bytes([0x80, 0x06]))
    devid = (rd[3] << 8) | rd[4]
    if devid == 0x3180:
        print("PASS: config read device id = 0x%04X" % devid)
    else:
        print("FAIL: config read device id = 0x%04X" % devid)
        ok = False

    # unknown command -> error
    rd = m.handle_frame(0x02, 0x99, b"")
    if rd[2] == ERROR_CODE:
        print("PASS: unknown command returns ERROR_CODE")
    else:
        print("FAIL: unknown command handling")
        ok = False

    print("SELFTEST:", "OK" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(_selftest())
