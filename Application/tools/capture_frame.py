#!/usr/bin/env python3
import serial, struct, sys

POLY, INIT, XOROUT, SYNC = 0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF, 0xAA

def crc32_team(data):
    crc = INIT
    for b in data:
        crc ^= (b << 24)
        for _ in range(32):
            if crc & 0x80000000:
                crc = ((crc << 1) ^ POLY) & 0xFFFFFFFF
            else:
                crc = (crc << 1) & 0xFFFFFFFF
    return crc ^ XOROUT

s = serial.Serial("/dev/ttyUSB0", 921600, timeout=2)
print("Listening... (Ctrl-C to stop)")

while True:
    b = s.read(1)
    if not b or b[0] != SYNC:
        continue
    rest = s.read(2)
    if len(rest) < 2:
        continue
    ftype, flen = rest[0], rest[1]
    if flen > 248:
        print(f"⚠ bad len={flen}, resyncing")
        continue
    body = s.read(flen + 4)
    if len(body) < flen + 4:
        continue

    payload = body[:flen]
    crc_rx  = struct.unpack(">I", body[flen:flen+4])[0]
    crc_calc = crc32_team(bytes([ftype, flen]) + payload)
    ok = "✅" if crc_rx == crc_calc else "❌"

    raw = bytes([SYNC, ftype, flen]) + body
    print(f"{ok} {raw.hex(' ').upper()}  "
          f"(type=0x{ftype:02X} len={flen} crc=0x{crc_rx:08X})")
