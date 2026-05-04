#!/usr/bin/env python3
"""Reference frame builder — must produce identical bytes to STM32 Frame_Build."""

POLY      = 0x04C11DB7
INIT      = 0xFFFFFFFF
XOROUT    = 0xFFFFFFFF
SYNC_BYTE = 0xAA

def crc32_team(data: bytes) -> int:
    """CRC32-MPEG2 with 32-iteration inner loop (matches teammate's ESP32)."""
    crc = INIT
    for b in data:
        crc ^= (b << 24)
        for _ in range(32):              # 32 iterations, NOT 8 — intentional
            if crc & 0x80000000:
                crc = ((crc << 1) ^ POLY) & 0xFFFFFFFF
            else:
                crc = (crc << 1) & 0xFFFFFFFF
    return crc ^ XOROUT

def build_frame(ftype: int, payload: bytes) -> bytes:
    if len(payload) > 248:
        raise ValueError("payload too big")
    header = bytes([SYNC_BYTE, ftype, len(payload)])
    crc_input = header[1:] + payload          # TYPE + LEN + PAYLOAD
    crc = crc32_team(crc_input)
    crc_be = crc.to_bytes(4, "big")
    return header + payload + crc_be

# ---- Test vector matching test_frame_build() on the STM32 ----
TYPE_CLASSIFICATION = 0x01
payload = bytes([0x01, 0x5C, 0x12, 0x34, 0x56, 0x78])

frame = build_frame(TYPE_CLASSIFICATION, payload)

print(f"Frame length : {len(frame)} bytes")
print(f"Frame hex    : {frame.hex(' ').upper()}")
print(f"CRC32 (hex)  : 0x{crc32_team(bytes([TYPE_CLASSIFICATION, len(payload)]) + payload):08X}")
print(f"CRC bytes BE : {frame[-4:].hex(' ').upper()}")
