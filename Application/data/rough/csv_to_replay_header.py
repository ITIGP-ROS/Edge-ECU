#!/usr/bin/env python3
"""
csv_to_replay_header.py
=======================
Convert a road-surface CSV recording into a C header file containing
a const MPU6050_RawData_t array, suitable for replay testing on the MCU.

Usage:
    python3 csv_to_replay_header.py input.csv output.h
    python3 csv_to_replay_header.py input.csv output.h --start 100 --count 200

The CSV is expected to have:
    - Metadata lines starting with '#' at the top
    - One header row with column names
    - Data rows with: tick_ms, ax_raw, ay_raw, az_raw, gx_raw, gy_raw, gz_raw, seq
"""

import sys
import csv
import argparse
from datetime import datetime


def parse_args():
    p = argparse.ArgumentParser(description="CSV → C header replay generator")
    p.add_argument("input_csv",  help="Input CSV file")
    p.add_argument("output_h",   help="Output .h file")
    p.add_argument("--start", type=int, default=0,
                   help="Skip N data rows before sampling (default: 0)")
    p.add_argument("--count", type=int, default=200,
                   help="Number of samples to emit (default: 200)")
    return p.parse_args()


def read_csv(path, start, count):
    """Return list of (ax,ay,az,gx,gy,gz) tuples + metadata dict."""
    metadata = {}
    rows = []

    with open(path, "r", encoding="utf-8") as f:
        reader = csv.reader(f)
        header_seen = False

        for row in reader:
            if not row:
                continue

            # Metadata line (starts with '#')
            if row[0].startswith("#"):
                # e.g. "# class=smooth"
                kv = row[0].lstrip("#").strip()
                if "=" in kv:
                    k, v = kv.split("=", 1)
                    metadata[k.strip()] = v.strip()
                continue

            # Header row — first non-comment row
            if not header_seen:
                header_seen = True
                continue

            # Data row
            try:
                # Columns: tick_ms, ax, ay, az, gx, gy, gz, seq
                ax = int(row[1])
                ay = int(row[2])
                az = int(row[3])
                gx = int(row[4])
                gy = int(row[5])
                gz = int(row[6])
                rows.append((ax, ay, az, gx, gy, gz))
            except (ValueError, IndexError):
                # Skip malformed rows silently
                continue

    # Apply start/count slicing
    end = start + count
    selected = rows[start:end]

    if len(selected) < count:
        print(f"WARNING: requested {count} samples but only "
              f"{len(selected)} available (after skipping {start}).",
              file=sys.stderr)

    return selected, metadata


def write_header(path, samples, metadata, source_csv):
    """Emit a C99 header with the const array."""
    n = len(samples)
    now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    with open(path, "w", encoding="utf-8") as f:
        f.write("/*\n")
        f.write(" * replay_data.h — auto-generated, do not edit by hand.\n")
        f.write(f" * Generated:    {now}\n")
        f.write(f" * Source CSV:   {source_csv}\n")
        f.write(f" * Samples:      {n}\n")
        f.write(" *\n")
        f.write(" * CSV metadata:\n")
        for k, v in metadata.items():
            f.write(f" *   {k} = {v}\n")
        f.write(" */\n\n")

        f.write("#ifndef REPLAY_DATA_H\n")
        f.write("#define REPLAY_DATA_H\n\n")
        f.write('#include "MPU6050.h"\n\n')

        f.write(f"#define REPLAY_NUM_SAMPLES   {n}U\n\n")

        # Expected ground-truth label (from metadata)
        cls = metadata.get("class", "unknown")
        f.write(f"/* Expected ground-truth label: \"{cls}\" */\n")
        f.write(f"#define REPLAY_EXPECTED_CLASS_STR \"{cls}\"\n\n")

        f.write("static const MPU6050_RawData_t replay_samples[REPLAY_NUM_SAMPLES] = {\n")
        for i, (ax, ay, az, gx, gy, gz) in enumerate(samples):
            f.write(f"    /* [{i:3d}] */ {{ "
                    f".accel_x = {ax:6d}, "
                    f".accel_y = {ay:6d}, "
                    f".accel_z = {az:6d}, "
                    f".temp_raw = 0, "
                    f".gyro_x  = {gx:6d}, "
                    f".gyro_y  = {gy:6d}, "
                    f".gyro_z  = {gz:6d} }},\n")
        f.write("};\n\n")

        f.write("#endif /* REPLAY_DATA_H */\n")


def main():
    args = parse_args()
    samples, metadata = read_csv(args.input_csv, args.start, args.count)

    if not samples:
        print("ERROR: no samples extracted!", file=sys.stderr)
        sys.exit(1)

    write_header(args.output_h, samples, metadata, args.input_csv)
    print(f"✅ Wrote {len(samples)} samples to {args.output_h}")
    print(f"   Source: {args.input_csv}")
    print(f"   Class:  {metadata.get('class', 'unknown')}")


if __name__ == "__main__":
    main()
