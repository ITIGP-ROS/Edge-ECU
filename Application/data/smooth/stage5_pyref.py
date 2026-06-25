#!/usr/bin/env python3
"""
stage5_pyref.py — Python reference feature extractor for cross-check.
Mirrors features.c logic: 50 features per 50-sample window of 6 channels.
"""
import csv
import sys
import numpy as np

ACCEL_SCALE = 8192.0
GYRO_SCALE  = 65.5
WINDOW_SIZE = 50
N_FEATURES  = 6
HALF_WINDOW = 25
FFT_N       = 64
FFT_HF_SPLIT = 16
EPS = 1e-10

def feat_std(x):  return np.std(x)            # population (matches numpy default ddof=0)
def feat_mad(x):  return np.mean(np.abs(np.diff(x))) if len(x) > 1 else 0.0
def feat_p2p(x):  return np.ptp(x)
def feat_rms(x):  return np.sqrt(np.mean(x*x))
def feat_iqr(x):  return np.percentile(x, 75) - np.percentile(x, 25)

def feat_skew(x):
    m, s = np.mean(x), np.std(x)
    if s < EPS: return 0.0
    z = (x - m) / s
    return np.mean(z**3)

def feat_corr(x, y):
    sx, sy = np.std(x), np.std(y)
    if sx < EPS or sy < EPS: return 0.0
    return np.corrcoef(x, y)[0, 1]

def feat_hfe(x):
    """Match CMSIS-DSP rfft: zero-pad to FFT_N, compute |X|^2, ratio of bins>=16."""
    padded = np.zeros(FFT_N)
    padded[:WINDOW_SIZE] = x
    spectrum = np.fft.rfft(padded)
    mag_sq = np.abs(spectrum) ** 2     # 33 bins for FFT_N=64
    total = mag_sq.sum() + EPS
    hfe = mag_sq[FFT_HF_SPLIT:].sum()
    return hfe / total

def extract_features(scaled):
    """scaled shape: (50, 6) — columns ax, ay, az, gx, gy, gz, all in g/dps."""
    f = np.zeros(50)

    # Per-channel features 0..35 (6 channels × 6 features each)
    for ch in range(6):
        x = scaled[:, ch]
        b = ch * 6
        f[b+0] = feat_std(x)
        f[b+1] = feat_mad(x)
        f[b+2] = feat_p2p(x)
        f[b+3] = feat_hfe(x)
        f[b+4] = feat_iqr(x)
        f[b+5] = feat_rms(x)

    # Magnitudes
    amag = np.sqrt(scaled[:,0]**2 + scaled[:,1]**2 + scaled[:,2]**2)
    gmag = np.sqrt(scaled[:,3]**2 + scaled[:,4]**2 + scaled[:,5]**2)

    f[36] = feat_std(amag);  f[37] = feat_mad(amag)
    f[38] = feat_p2p(amag);  f[39] = feat_rms(amag)
    f[40] = feat_std(gmag);  f[41] = feat_mad(gmag)
    f[42] = feat_p2p(gmag);  f[43] = feat_rms(gmag)

    # Correlations
    f[44] = feat_corr(amag, gmag)
    f[45] = feat_corr(scaled[:,2], scaled[:,5])

    # Skewness
    f[46] = feat_skew(scaled[:,2])
    f[47] = feat_skew(scaled[:,5])

    # Variance ratios
    var_a_lo = np.var(amag[:HALF_WINDOW]); var_a_hi = np.var(amag[HALF_WINDOW:])
    var_g_lo = np.var(gmag[:HALF_WINDOW]); var_g_hi = np.var(gmag[HALF_WINDOW:])
    f[48] = (var_a_hi + EPS) / (var_a_lo + EPS)
    f[49] = (var_g_hi + EPS) / (var_g_lo + EPS)

    # Sanitize
    f = np.where(np.isnan(f), 0.0, f)
    f = np.where(np.isposinf(f), 10.0, f)
    f = np.where(np.isneginf(f), -10.0, f)
    return f

def read_csv_window(path, start=0, count=50):
    rows = []
    with open(path) as fh:
        reader = csv.reader(fh)
        seen_header = False
        for row in reader:
            if not row or row[0].startswith("#"): continue
            if not seen_header: seen_header = True; continue
            try:
                rows.append([int(row[1]), int(row[2]), int(row[3]),
                             int(row[4]), int(row[5]), int(row[6])])
            except: continue
    arr = np.array(rows[start:start+count], dtype=np.float32)
    return arr

if __name__ == "__main__":
    csv_path = sys.argv[1]
    raw = read_csv_window(csv_path, 0, 50)
    scaled = raw.copy()
    scaled[:, 0:3] /= ACCEL_SCALE
    scaled[:, 3:6] /= GYRO_SCALE

    features = extract_features(scaled)
    print("# Python reference features (raw, before normalize)")
    for i, v in enumerate(features):
        print(f"feat[{i:2d}] = {v:+.6f}")
