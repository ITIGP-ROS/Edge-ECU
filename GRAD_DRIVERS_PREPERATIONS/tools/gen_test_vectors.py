#!/usr/bin/env python3
"""
Generate tools/test_vectors.h from real training data.

Picks 5 windows (2 smooth + 3 rough) from ../data/{smooth,rough}/*.csv,
seeded with RANDOM_SEED=42 for reproducibility, and emits a C header
containing:

  - test_raw            : int16  raw IMU samples
  - expected_scaled     : float  output of the C scaler stage
  - expected_features   : float  50-element feature vector (extract_features)
  - expected_ts_int8    : int8   row-major quantized time-series tensor
  - expected_stat_int8  : int8   quantized stat-feature tensor (after norm)
  - expected_labels     : int    ground-truth labels (0=smooth, 1=rough)

Quantization params (TS_SCALE, TS_ZP, STAT_SCALE, STAT_ZP) are parsed from
../include/norm_params.h so this script tracks the firmware constants
automatically. Stat normalization params are loaded from
../models/stat_mean.npy and ../models/stat_std.npy.

Usage (from tools/ directory):
    python3 gen_test_vectors.py
"""

# =========================================================================
# IMPORTANT — extract_features()
# =========================================================================
# The extract_features() function below MUST be byte-for-byte identical to
# the one in train_model.ipynb. The skeleton at the bottom of this file
# implements it as documented in features.c — but if your notebook differs
# (e.g. different FFT size, different feature ordering, scipy.skew vs
# manual skew), PASTE THE NOTEBOOK VERSION OVER IT.
# =========================================================================
#
# IMPORTANT — stat_std clamp threshold
# =========================================================================
# The STAT_STD_CLAMP threshold below must match BOTH the trainer
# (train_model.ipynb) AND the firmware (stat_norm.c reciprocal bake).
# If they disagree, expected_stat_int8 will diverge from the MCU output
# by ±1 LSB on near-zero-std features. See comments inline.
# =========================================================================

import glob
import random
import re
import sys
from pathlib import Path

import numpy as np
import pandas as pd

# ============================================================================
# Configuration
# ============================================================================
RANDOM_SEED  = 42
WINDOW_SIZE  = 50
N_FEATURES   = 6
N_STAT       = 50
ACCEL_SCALE  = 8192.0
GYRO_SCALE   = 65.5
FFT_N        = 64
FFT_HF_SPLIT = 16

# Must match the trainer + firmware. Diverging by even one decimal place
# will silently produce ±1 LSB STAT_INT8 mismatches on dead-channel features.
STAT_STD_CLAMP = 1e-8

DATA_DIR    = Path("../data")
MODELS_DIR  = Path("../include")   # norm_params.h now lives in include/
NPY_DIR     = Path("../models")    # stat_mean.npy / stat_std.npy
OUT_FILE    = Path("test_vectors.h")

CSV_COLUMNS = ['ax_raw', 'ay_raw', 'az_raw', 'gx_raw', 'gy_raw', 'gz_raw']

# ============================================================================
# Parse norm_params.h
# ============================================================================
def parse_norm_params():
    """Read TS_SCALE / TS_ZP / STAT_SCALE / STAT_ZP from norm_params.h."""
    fp = MODELS_DIR / "norm_params.h"
    if not fp.exists():
        # Fallback: maybe it's still in ../models/ for legacy layouts.
        alt = Path("../models/norm_params.h")
        if alt.exists():
            fp = alt
        else:
            sys.exit(f"ERROR: cannot find norm_params.h "
                     f"(looked in {MODELS_DIR}/ and ../models/)")

    txt = fp.read_text()

    def grab(name):
        m = re.search(rf"#define\s+{name}\s+([-+0-9.eEf]+)", txt)
        if m is None:
            sys.exit(f"ERROR: {name} not found in {fp}")
        return float(m.group(1).rstrip('f'))

    return {
        'TS_SCALE'  : grab('TS_SCALE'),
        'TS_ZP'     : int(grab('TS_ZP')),
        'STAT_SCALE': grab('STAT_SCALE'),
        'STAT_ZP'   : int(grab('STAT_ZP')),
    }

# ============================================================================
# Load stat_mean / stat_std from npy
# ============================================================================
def load_stat_norm():
    """Load stat_mean.npy and stat_std.npy from ../models/."""
    mp = NPY_DIR / "stat_mean.npy"
    sp = NPY_DIR / "stat_std.npy"
    if not mp.exists():
        sys.exit(f"ERROR: missing {mp}")
    if not sp.exists():
        sys.exit(f"ERROR: missing {sp}")
    mean = np.load(mp).astype(np.float32)
    std  = np.load(sp).astype(np.float32)
    if mean.shape != (N_STAT,) or std.shape != (N_STAT,):
        sys.exit(f"ERROR: stat_mean shape {mean.shape} or stat_std shape "
                 f"{std.shape} != ({N_STAT},)")
    return mean, std

# ============================================================================
# CSV reader — matches train_model.ipynb / record.py format
# ============================================================================
def load_csv(fp):
    """
    Load a recording CSV.

    Format expected:
      - Optional '#' comment lines at the top
      - Header row with at least: ax_raw, ay_raw, az_raw, gx_raw, gy_raw, gz_raw
      - Data rows of signed int16 values

    Returns a DataFrame containing only the 6 raw IMU columns, in canonical
    order, with dtype int16.
    """
    df = pd.read_csv(fp, comment='#')
    missing = [c for c in CSV_COLUMNS if c not in df.columns]
    if missing:
        sys.exit(f"ERROR: {fp} missing columns: {missing}")
    df = df[CSV_COLUMNS].copy()
    df = df.dropna().reset_index(drop=True)
    df = df.astype(np.int16)
    return df

# ============================================================================
# extract_features() — must match train_model.ipynb (and features.c)
# ============================================================================
def _hfe(sig):
    """High-frequency energy ratio — 64-pt zero-padded FFT, bins >= 16."""
    sig_padded = np.zeros(FFT_N, dtype=np.float32)
    sig_padded[:WINDOW_SIZE] = sig.astype(np.float32)
    fft_e = np.abs(np.fft.rfft(sig_padded)) ** 2
    total = np.sum(fft_e) + 1e-10
    return float(np.sum(fft_e[FFT_HF_SPLIT:]) / total)

def _skew(x):
    m = float(np.mean(x))
    s = float(np.std(x))               # population std (ddof=0)
    if s < 1e-10:
        return 0.0
    return float(np.mean(((x - m) / s) ** 3))

def _corr(x, y):
    if np.std(x) < 1e-10 or np.std(y) < 1e-10:
        return 0.0
    return float(np.corrcoef(x, y)[0, 1])

def extract_features_one(scaled):
    """
    Compute the 50-element feature vector for ONE scaled window.

    Input:  scaled[50, 6] float32 (channels: ax, ay, az, gx, gy, gz)
    Output: f[50]         float32

    Matches features.c element-for-element.
    """
    f = np.zeros(N_STAT, dtype=np.float32)

    # Per-channel features 0..35 — [std, mad, p2p, hfe, iqr, rms]
    for ch in range(N_FEATURES):
        x = scaled[:, ch]
        b = 6 * ch
        f[b + 0] = np.std(x)
        f[b + 1] = np.mean(np.abs(np.diff(x)))
        f[b + 2] = np.ptp(x)
        f[b + 3] = _hfe(x)
        f[b + 4] = (np.percentile(x, 75) -
                    np.percentile(x, 25))
        f[b + 5] = np.sqrt(np.mean(x ** 2))

    # Magnitude features 36..43
    amag = np.sqrt(np.sum(scaled[:, 0:3] ** 2, axis=1))
    gmag = np.sqrt(np.sum(scaled[:, 3:6] ** 2, axis=1))

    f[36] = np.std(amag)
    f[37] = np.mean(np.abs(np.diff(amag)))
    f[38] = np.ptp(amag)
    f[39] = np.sqrt(np.mean(amag ** 2))

    f[40] = np.std(gmag)
    f[41] = np.mean(np.abs(np.diff(gmag)))
    f[42] = np.ptp(gmag)
    f[43] = np.sqrt(np.mean(gmag ** 2))

    # Correlations 44..45
    f[44] = _corr(amag, gmag)
    f[45] = _corr(scaled[:, 2], scaled[:, 5])

    # Skewness 46..47
    f[46] = _skew(scaled[:, 2])
    f[47] = _skew(scaled[:, 5])

    # Variance ratios 48..49 (second half / first half, both stabilised)
    half = WINDOW_SIZE // 2
    f[48] = ((np.var(amag[half:]) + 1e-10) /
             (np.var(amag[:half]) + 1e-10))
    f[49] = ((np.var(gmag[half:]) + 1e-10) /
             (np.var(gmag[:half]) + 1e-10))

    f = np.nan_to_num(f, nan=0.0, posinf=10.0, neginf=-10.0)
    return f.astype(np.float32)

def extract_features(X_windows):
    """Vectorized over a batch of windows. Input shape (N, 50, 6)."""
    out = np.zeros((X_windows.shape[0], N_STAT), dtype=np.float32)
    for i in range(X_windows.shape[0]):
        out[i] = extract_features_one(X_windows[i])
    return out

# ============================================================================
# Stat normalization — replicates Quantize_NormalizeStat in firmware
# ============================================================================
def normalize_stat(feats, stat_mean, stat_std):
    """(feats - mean) / clamp(std, STAT_STD_CLAMP)."""
    safe = np.where(stat_std < STAT_STD_CLAMP, 1.0, stat_std)
    return (feats - stat_mean) / safe

# ============================================================================
# Window picker
# ============================================================================
def pick_windows():
    """Pick 2 smooth + 3 rough windows, seeded for reproducibility."""
    random.seed(RANDOM_SEED)
    np.random.seed(RANDOM_SEED)

    smooth_files = sorted(glob.glob(str(DATA_DIR / "smooth" / "*.csv")))
    rough_files  = sorted(glob.glob(str(DATA_DIR / "rough"  / "*.csv")))

    if len(smooth_files) < 2:
        sys.exit(f"ERROR: need >=2 smooth CSVs in {DATA_DIR}/smooth/, "
                 f"found {len(smooth_files)}")
    if len(rough_files) < 3:
        sys.exit(f"ERROR: need >=3 rough CSVs in {DATA_DIR}/rough/, "
                 f"found {len(rough_files)}")

    picks = []  # list of (label_int, source_str, raw_int16[50, 6])

    def _pick_from(files, label, n):
        chosen = random.sample(files, n)
        for fp in chosen:
            df = load_csv(fp)
            if len(df) < WINDOW_SIZE + 1:
                sys.exit(f"ERROR: {fp} has only {len(df)} rows, "
                         f"need >= {WINDOW_SIZE + 1}")
            off = random.randint(0, len(df) - WINDOW_SIZE - 1)
            raw = df[CSV_COLUMNS].values[off:off + WINDOW_SIZE].astype(np.int16)
            label_dir = "smooth" if label == 0 else "rough"
            src = f"{label_dir}/{Path(fp).name}@offset_{off}"
            picks.append((label, src, raw))

    _pick_from(smooth_files, label=0, n=2)
    _pick_from(rough_files,  label=1, n=3)

    return picks

# ============================================================================
# Quantization (matches Quantize_TS / Quantize_Stat in C)
# ============================================================================
def quantize_int8(values, scale, zero_point):
    """
    TFLite-style affine quantization.

    Uses np.round() = round-half-to-even (banker's rounding), which matches
    nearbyintf() with the default FE_TONEAREST rounding mode on Cortex-M4F.
    If your quantize.c uses round-half-away-from-zero (the
    `(x + 0.5)` idiom), you'll see ±1 LSB diffs at exact half-values —
    bump the verify.c tolerance to 1 LSB or change one side to match.
    """
    q = np.round(values / scale).astype(np.int32) + zero_point
    return np.clip(q, -128, 127).astype(np.int8)

# ============================================================================
# C-formatting helpers
# ============================================================================
def fmt_int16_window(raw):
    """Format a (50, 6) int16 array as a brace-nested C initializer."""
    rows = []
    for t in range(WINDOW_SIZE):
        cells = ", ".join(f"{int(v):6d}" for v in raw[t])
        rows.append(f"        {{ {cells} }}")
    return "{\n" + ",\n".join(rows) + "\n    }"

def fmt_float_window(scaled):
    """Format a (50, 6) float array as a brace-nested C initializer."""
    rows = []
    for t in range(WINDOW_SIZE):
        cells = []
        for v in scaled[t]:
            v = float(v)
            if not np.isfinite(v):
                v = 0.0
            cells.append(f"{v: .8f}f")
        rows.append("        { " + ", ".join(cells) + " }")
    return "{\n" + ",\n".join(rows) + "\n    }"

def fmt_float_array(arr):
    cells = []
    for v in arr:
        v = float(v)
        if not np.isfinite(v):
            v = 0.0
        cells.append(f"{v: .8f}f")
    return "{ " + ", ".join(cells) + " }"

def fmt_int8_array(arr):
    return "{ " + ", ".join(f"{int(v):4d}" for v in arr) + " }"

# ============================================================================
# Main
# ============================================================================
def main():
    params = parse_norm_params()
    print(f"Parsed quant params: TS_SCALE={params['TS_SCALE']}, "
          f"TS_ZP={params['TS_ZP']}, "
          f"STAT_SCALE={params['STAT_SCALE']}, "
          f"STAT_ZP={params['STAT_ZP']}",
          file=sys.stderr)

    stat_mean, stat_std = load_stat_norm()
    print(f"Loaded stat_mean / stat_std from {NPY_DIR}/  "
          f"(clamp threshold = {STAT_STD_CLAMP:g})",
          file=sys.stderr)

    picks = pick_windows()
    print(f"Selected {len(picks)} windows:", file=sys.stderr)
    for label, src, _ in picks:
        kind = "smooth" if label == 0 else "rough"
        print(f"  [{kind}] {src}", file=sys.stderr)

    # --- precompute per-vector intermediates so we don't recompute 4x below ---
    intermediates = []  # list of dicts
    for label, src, raw in picks:
        scaled = raw.astype(np.float32).copy()
        scaled[:, 0:3] /= np.float32(ACCEL_SCALE)
        scaled[:, 3:6] /= np.float32(GYRO_SCALE)

        feats      = extract_features(scaled[np.newaxis, :, :])[0]
        feats_norm = normalize_stat(feats, stat_mean, stat_std)
        ts_q       = quantize_int8(scaled.reshape(-1),
                                   params['TS_SCALE'], params['TS_ZP'])
        stat_q     = quantize_int8(feats_norm,
                                   params['STAT_SCALE'], params['STAT_ZP'])

        intermediates.append({
            'label':  label,
            'src':    src,
            'raw':    raw,
            'scaled': scaled,
            'feats':  feats,
            'ts_q':   ts_q,
            'stat_q': stat_q,
        })

    # --- emit header ---
    out = []
    out.append("#pragma once")
    out.append("/* Auto-generated by tools/gen_test_vectors.py - DO NOT EDIT */")
    out.append("")
    out.append("#include <stdint.h>")
    out.append("")
    out.append(f"#define N_TEST_VECTORS {len(picks)}")
    out.append("")

    # ----- expected_labels (renamed from test_labels for tooling consistency) -----
    labels_str = ", ".join(str(d['label']) for d in intermediates)
    out.append("/* Ground-truth labels: 0=smooth, 1=rough */")
    out.append(f"static const int expected_labels[N_TEST_VECTORS] = "
               f"{{ {labels_str} }};")
    out.append("")

    # ----- sources -----
    out.append("/* Source file + offset (for traceability) */")
    out.append("static const char *test_sources[N_TEST_VECTORS] = {")
    for d in intermediates:
        out.append(f'    "{d["src"]}",')
    out.append("};")
    out.append("")

    # ----- raw int16 windows -----
    out.append("/* Raw int16 IMU samples */")
    out.append(f"static const int16_t test_raw[N_TEST_VECTORS]"
               f"[{WINDOW_SIZE}][{N_FEATURES}] = {{")
    for d in intermediates:
        out.append("    " + fmt_int16_window(d['raw']) + ",")
    out.append("};")
    out.append("")

    # ----- expected_scaled -----
    out.append("/* Expected float32 scaled values (raw / scale) */")
    out.append(f"static const float expected_scaled[N_TEST_VECTORS]"
               f"[{WINDOW_SIZE}][{N_FEATURES}] = {{")
    for d in intermediates:
        out.append("    " + fmt_float_window(d['scaled']) + ",")
    out.append("};")
    out.append("")

    # ----- expected_features (50 floats) -----
    out.append("/* Expected feature vector (50 floats) - for MCU verification */")
    out.append(f"static const float expected_features[N_TEST_VECTORS]"
               f"[{N_STAT}] = {{")
    for d in intermediates:
        out.append("    " + fmt_float_array(d['feats']) + ",")
    out.append("};")
    out.append("")

    # ----- expected_ts_int8 (300 bytes per window, row-major t*6+ch) -----
    out.append("/* Expected int8 quantized time-series (row-major: t*6 + ch) */")
    out.append(f"static const int8_t expected_ts_int8[N_TEST_VECTORS]"
               f"[{WINDOW_SIZE * N_FEATURES}] = {{")
    for d in intermediates:
        out.append("    " + fmt_int8_array(d['ts_q']) + ",")
    out.append("};")
    out.append("")

    # ----- expected_stat_int8 (50 bytes per window) -----
    out.append("/* Expected int8 quantized stat features.")
    out.append(" * Pipeline: extract_features -> (f - stat_mean) / clamp(stat_std, "
               f"{STAT_STD_CLAMP:g}) -> quantize.")
    out.append(" * Depends on stat_mean.npy / stat_std.npy in ../models/. "
               "If those")
    out.append(" * change, regenerate this header. */")
    out.append(f"static const int8_t expected_stat_int8[N_TEST_VECTORS]"
               f"[{N_STAT}] = {{")
    for d in intermediates:
        out.append("    " + fmt_int8_array(d['stat_q']) + ",")
    out.append("};")
    out.append("")

    OUT_FILE.write_text("\n".join(out))
    print(f"\nWrote {OUT_FILE.resolve()} ({len(picks)} vectors, "
          f"now with expected_stat_int8 + expected_labels)",
          file=sys.stderr)

if __name__ == "__main__":
    main()
