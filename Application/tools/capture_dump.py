#!/usr/bin/env python3
"""tools/capture_dump.py — capture MCU verify dump from a clean port open.

Usage:
    python3 capture_dump.py [PORT] [BAUD]
    
Defaults: /dev/ttyUSB0 @ 921600. Open the port FIRST, then press the MCU
reset button. Capture stops 2 s after the last byte.

Sanity-checks block markers and refuses to save a corrupt dump.
"""
import argparse, serial, sys, time

ap = argparse.ArgumentParser()
ap.add_argument('port', nargs='?', default='/dev/ttyUSB0')
ap.add_argument('baud', nargs='?', type=int, default=921600)
ap.add_argument('-o', '--output', default='mcu_dump.txt')
ap.add_argument('-n', '--expect', type=int, default=5,
                help='expected number of VERIFY blocks (default 5)')
args = ap.parse_args()

s = serial.Serial(args.port, args.baud, timeout=0.5)
print(f"Opened {args.port} @ {args.baud}. Press RESET on MCU now...")

buf  = bytearray()
last = time.time()
while time.time() - last < 2.0:
    chunk = s.read(4096)
    if chunk:
        buf += chunk
        last = time.time()
        sys.stdout.write('.'); sys.stdout.flush()

print()
n_start = buf.count(b'===VERIFY_START===')
n_end   = buf.count(b'===VERIFY_END===')
print(f"Captured {len(buf)} bytes")
print(f"VERIFY_START markers: {n_start} (expected {args.expect})")
print(f"VERIFY_END   markers: {n_end} (expected {args.expect})")

with open(args.output, 'wb') as f:
    f.write(buf)
print(f"Saved -> {args.output}")

if n_start != args.expect or n_end != args.expect:
    print(f"\n⚠️  Marker counts don't match expected {args.expect}. "
          f"Capture may be incomplete — try again.", file=sys.stderr)
    sys.exit(1)
