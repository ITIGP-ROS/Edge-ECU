#!/usr/bin/env python3
"""
tools/tflite_reference.py

Computes the Python-side reference values for ALL test vectors in
test_vectors.h:
  - quantized time-series tensor       (300 int8 per vector)
  - quantized stat-feature tensor       (50 int8 per vector)
  - TFLite int8 inference output        (2 int8 per vector)
  - argmax label + integer confidence   (matches firmware formula)

Run from tools/ directory. Compares against mcu_dump.txt if present.

Multi-block — handles all N test vectors in one pass.
"""

import re
import sys
from pathlib import Path

import numpy as np
import tensorflow as tf

# ---------------------------------------------------------------------------
# Constants — must match include/norm_params.h on the firmware side
# ---------------------------------------------------------------------------
ACCEL_SCALE = 8192.0
GYRO_SCALE  = 65.5
TS_SCALE    = 0.18967218697071075
TS_ZP       = -6
STAT_SCALE  = 0.05732841417193413
STAT_ZP     = -53

WINDOW_SIZE   = 50
N_FEATURES    = 6
N_STAT        = 50
N_TS          = WINDOW_SIZE * N_FEATURES   # 300

VEC_H         = Path("test_vectors.h")
TFLITE_MODEL  = Path("../models/model_road_int8.tflite")
STAT_MEAN_NPY = Path("../models/stat_mean.npy")
STAT_STD_NPY  = Path("../models/stat_std.npy")

# Ground-truth labels: from gen_test_vectors.py seed=42 → 2 smooth + 3 rough
DEFAULT_LABELS = [0, 0, 1, 1, 1]

VERBOSE = False   # flip to True to dump full 50-byte stat_q per vector


# ---------------------------------------------------------------------------
# C-source helpers
# ---------------------------------------------------------------------------
def _strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//[^\n]*", "", text)
    return text


def extract_all_inner_blocks(body):
    """Return a list of strings, each being the body of one balanced
    top-level `{...}` group in `body`. Used to walk
    `expected_xxx[N_TEST_VECTORS][...]` initializers."""
    blocks = []
    depth  = 0
    start  = -1
    for i, c in enumerate(body):
        if c == "{":
            if depth == 0:
                start = i + 1
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0 and start >= 0:
                blocks.append(body[start:i])
                start = -1
    return blocks


def find_array_body(text, var_name):
    """Locate `<var_name> ... = { <body> };` and return <body>, else None."""
    pat = re.compile(
        rf"\b{re.escape(var_name)}\b[^=;{{]*=\s*\{{(.*?)\}}\s*;",
        re.DOTALL,
    )
    m = pat.search(text)
    return m.group(1) if m else None


# Float pattern from the original script — handles scientific notation +
# optional 'f' suffix.
FLOAT_PAT = re.compile(
    r"[-+]?\d*\.\d+(?:[eE][-+]?\d+)?[fF]?"
    r"|[-+]?\d+\.\d*(?:[eE][-+]?\d+)?[fF]?"
    r"|[-+]?\d+[eE][-+]?\d+[fF]?"
)


# ---------------------------------------------------------------------------
# Parse test_vectors.h — all vectors
# ---------------------------------------------------------------------------
def load_test_vectors(path):
    """Return (raw_list, feats_list, labels) where:
       raw_list[i]  : np.ndarray shape (50, 6) int16
       feats_list[i]: np.ndarray shape (50,)   float32
       labels       : list[int] length N
    """
    if not path.exists():
        sys.exit(f"ERROR: {path} not found — run from tools/ directory")

    text = _strip_comments(path.read_text())

    raw_body = find_array_body(text, "test_raw")
    if raw_body is None:
        sys.exit("ERROR: test_raw not found in test_vectors.h")
    raw_blocks = extract_all_inner_blocks(raw_body)
    if not raw_blocks:
        sys.exit("ERROR: could not extract any test_raw[] blocks")

    feat_body = find_array_body(text, "expected_features")
    if feat_body is None:
        sys.exit("ERROR: expected_features not found in test_vectors.h")
    feat_blocks = extract_all_inner_blocks(feat_body)
    if not feat_blocks:
        sys.exit("ERROR: could not extract any expected_features[] blocks")

    if len(raw_blocks) != len(feat_blocks):
        sys.exit(f"ERROR: test_raw has {len(raw_blocks)} vectors but "
                 f"expected_features has {len(feat_blocks)}")

    raw_list   = []
    feats_list = []
    for i, (rb, fb) in enumerate(zip(raw_blocks, feat_blocks)):
        nums = [int(x) for x in re.findall(r"-?\d+", rb)]
        if len(nums) < N_TS:
            sys.exit(f"ERROR: test_raw[{i}] has {len(nums)} ints, "
                     f"need {N_TS}")
        raw_list.append(np.array(nums[:N_TS], dtype=np.int16)
                        .reshape(WINDOW_SIZE, N_FEATURES))

        toks = [t.rstrip("fF") for t in FLOAT_PAT.findall(fb)]
        if len(toks) < N_STAT:
            sys.exit(f"ERROR: expected_features[{i}] has {len(toks)} "
                     f"tokens, need {N_STAT}")
        feats_list.append(np.array([float(t) for t in toks[:N_STAT]],
                                   dtype=np.float32))

    # Optional expected_labels[] — fall back to DEFAULT_LABELS otherwise
    labels = list(DEFAULT_LABELS)
    lbl_body = find_array_body(text, "expected_labels")
    if lbl_body is not None:
        lbl_nums = [int(x) for x in re.findall(r"-?\d+", lbl_body)]
        if len(lbl_nums) >= len(raw_list):
            labels = lbl_nums[:len(raw_list)]
    if len(labels) < len(raw_list):
        # Pad if defaults are too short
        labels = labels + [0] * (len(raw_list) - len(labels))

    return raw_list, feats_list, labels[:len(raw_list)]


# ---------------------------------------------------------------------------
# Quantization helpers — mirror the firmware exactly
# ---------------------------------------------------------------------------
def quantize_ts(raw_int16):
    """Replicates Scale_RawWindow + Quantize_TS in firmware."""
    scaled = raw_int16.astype(np.float32).copy()
    scaled[:, 0:3] /= ACCEL_SCALE
    scaled[:, 3:6] /= GYRO_SCALE
    return np.clip(
        np.round(scaled / TS_SCALE).astype(np.int32) + TS_ZP,
        -128, 127,
    ).astype(np.int8)


def quantize_stat(feats, stat_mean, stat_std):
    """Replicates Quantize_NormalizeStat + Quantize_Stat in firmware.

    NOTE: the `< 1e-8` clamp threshold must match the trainer. If the
    trainer used a different threshold (or none), update here so this
    reference matches the firmware reciprocals baked at training time.
    """
    stat_std_safe = np.where(stat_std < 1e-8, 1.0, stat_std)
    feats_norm    = (feats - stat_mean) / stat_std_safe
    return np.clip(
        np.round(feats_norm / STAT_SCALE).astype(np.int32) + STAT_ZP,
        -128, 127,
    ).astype(np.int8)


def label_and_confidence(out_i8):
    """Replicates the firmware's argmax + integer-confidence formula."""
    o0 = int(out_i8[0])
    o1 = int(out_i8[1])
    if o1 > o0:
        label, winner, loser = 1, o1, o0
    else:
        label, winner, loser = 0, o0, o1
    margin     = max(0, min(255, winner - loser))
    confidence = (margin * 100) // 255
    return label, confidence, margin


# ---------------------------------------------------------------------------
# MCU dump parser (multi-block) — for reproducibility comparison
# ---------------------------------------------------------------------------
def parse_mcu_dump(path):
    """Return list of dicts: {vec, label, stat_int8, ts_int8, inference}."""
    if not path.exists():
        return None
    text = path.read_text(errors="replace")

    blocks   = []
    cur      = None
    start_re = re.compile(r"===VERIFY_START===,VEC=(\d+),LABEL=(\d+)")

    for raw in text.splitlines():
        line = raw.lstrip("\x00\xff\r\n\t ")

        m = start_re.search(line)
        if m:
            if cur is not None:
                blocks.append(cur)
            cur = {"vec":       int(m.group(1)),
                   "label":     int(m.group(2)),
                   "stat_int8": [None] * N_STAT,
                   "ts_int8":   [None] * N_TS,
                   "inference": None}
            continue

        if "===VERIFY_END===" in line:
            if cur is not None:
                blocks.append(cur)
                cur = None
            continue

        if cur is None:
            continue

        m = re.match(r"STAT_INT8,(.+)", line)
        if m:
            try:
                vals = [int(x) for x in m.group(1).split(",") if x.strip()]
            except ValueError:
                continue
            for i, v in enumerate(vals):
                if i < N_STAT:
                    cur["stat_int8"][i] = v
            continue

        m = re.match(r"TS_INT8_(\d+),(.+)", line)
        if m:
            idx = int(m.group(1))
            try:
                vals = [int(x) for x in m.group(2).split(",") if x.strip()]
            except ValueError:
                continue
            start = idx * 100
            for i, v in enumerate(vals):
                if start + i < N_TS:
                    cur["ts_int8"][start + i] = v
            continue

        m = re.match(r"INFERENCE,(\d+),(\d+)", line)
        if m:
            cur["inference"] = (int(m.group(1)), int(m.group(2)))
            continue

    return blocks


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    raw_list, feats_list, labels = load_test_vectors(VEC_H)
    n = len(raw_list)
    print(f"Loaded {n} test vector(s) from {VEC_H}")

    if not STAT_MEAN_NPY.exists() or not STAT_STD_NPY.exists():
        sys.exit(f"ERROR: missing {STAT_MEAN_NPY} or {STAT_STD_NPY}")
    stat_mean = np.load(STAT_MEAN_NPY).astype(np.float32)
    stat_std  = np.load(STAT_STD_NPY).astype(np.float32)

    if not TFLITE_MODEL.exists():
        sys.exit(f"ERROR: {TFLITE_MODEL} not found")

    interp = tf.lite.Interpreter(str(TFLITE_MODEL))
    interp.allocate_tensors()
    inputs  = interp.get_input_details()
    outputs = interp.get_output_details()

    print()
    for i, inp in enumerate(inputs):
        print(f"input[{i}]  name={inp['name']:<40s} "
              f"shape={inp['shape'].tolist()} dtype={inp['dtype'].__name__}")
    for i, out in enumerate(outputs):
        print(f"output[{i}] name={out['name']:<40s} "
              f"shape={out['shape'].tolist()} dtype={out['dtype'].__name__}")

    ts_idx   = next(i for i, inp in enumerate(inputs)
                    if "ts_input" in inp["name"])
    stat_idx = next(i for i, inp in enumerate(inputs)
                    if "stat_input" in inp["name"])
    out_scale, out_zp = outputs[0]["quantization"]

    # ---- per-vector compute ----
    py_results = []   # list of dicts: ts_q, stat_q, out_i8, label, conf, ...
    for i in range(n):
        ts_q   = quantize_ts(raw_list[i])
        stat_q = quantize_stat(feats_list[i], stat_mean, stat_std)

        interp.set_tensor(inputs[ts_idx  ]["index"], ts_q.reshape(1, 50, 6))
        interp.set_tensor(inputs[stat_idx]["index"], stat_q.reshape(1, 50))
        interp.invoke()
        out_i8 = interp.get_tensor(outputs[0]["index"])[0]

        label, conf, margin = label_and_confidence(out_i8)
        deq = [(int(o) - out_zp) * out_scale for o in out_i8]

        py_results.append({
            "ts_q":     ts_q,
            "stat_q":   stat_q,
            "out_i8":   [int(out_i8[0]), int(out_i8[1])],
            "deq":      deq,
            "label":    label,
            "conf":     conf,
            "margin":   margin,
            "gt_label": labels[i],
        })

    # ---- per-vector report ----
    print()
    print("=" * 68)
    print("  Python reference results")
    print("=" * 68)
    for i, r in enumerate(py_results):
        ok_gt = (r["label"] == r["gt_label"])
        tag   = "✅" if ok_gt else "⚠️ "
        print(f"\n── Vector {i}  (gt_label={r['gt_label']}) ──")
        print(f"  ts_q   first 10 : {r['ts_q'].flatten()[:10].tolist()}")
        if VERBOSE:
            print(f"  stat_q full     : {r['stat_q'].tolist()}")
        else:
            print(f"  stat_q first 8  : {r['stat_q'][:8].tolist()}")
            print(f"  stat_q last  8  : {r['stat_q'][-8:].tolist()}")
        print(f"  output int8     : {r['out_i8']}")
        print(f"  output deq      : [{r['deq'][0]:+.6f}, {r['deq'][1]:+.6f}]"
              f"  (scale={out_scale:.6f}, zp={out_zp})")
        print(f"  predicted label : {r['label']} "
              f"({'rough' if r['label'] == 1 else 'smooth'})")
        print(f"  confidence      : {r['conf']}/100  (margin={r['margin']})")
        print(f"  vs ground truth : {tag} "
              f"{'match' if ok_gt else 'MISPREDICT'}")

    # ---- summary table ----
    print()
    print("=" * 68)
    print("  Python reference summary")
    print("=" * 68)
    print(f"  {'VEC':>3}  {'GT':>3}  {'PRED':>4}  {'CONF':>4}  "
          f"{'OUT_I8':>14}  {'GT_MATCH':>9}")
    for i, r in enumerate(py_results):
        out_str = f"[{r['out_i8'][0]:+4d},{r['out_i8'][1]:+4d}]"
        gt_str  = "OK" if r["label"] == r["gt_label"] else "miss"
        print(f"  {i:>3}  {r['gt_label']:>3}  {r['label']:>4}  "
              f"{r['conf']:>4}  {out_str:>14}  {gt_str:>9}")

    # ---- compare against MCU dump if present ----
    dump_path = Path("mcu_dump.txt")
    mcu_blocks = parse_mcu_dump(dump_path)
    if mcu_blocks is None:
        print()
        print(f"(no {dump_path} found — skipping MCU reproducibility check)")
        return 0

    print()
    print("=" * 68)
    print(f"  Comparing MCU dump ({dump_path}) vs Python reference")
    print("=" * 68)

    by_vec = {b["vec"]: b for b in mcu_blocks}
    all_ok = True

    print(f"  {'VEC':>3}  {'STAT_INT8':>10}  {'TS_INT8':>10}  "
          f"{'INFERENCE':>14}")
    for i, r in enumerate(py_results):
        if i not in by_vec:
            print(f"  {i:>3}  {'<MISSING from dump>':>40}")
            all_ok = False
            continue
        m = by_vec[i]

        # STAT_INT8 reproducibility (MCU vs Python)
        stat_diffs = [(j, m["stat_int8"][j], int(r["stat_q"][j]))
                      for j in range(N_STAT)
                      if (m["stat_int8"][j] is None
                          or m["stat_int8"][j] != int(r["stat_q"][j]))]
        stat_ok = (len(stat_diffs) == 0)

        # TS_INT8 reproducibility
        ts_flat   = r["ts_q"].flatten()
        ts_diffs  = [(j, m["ts_int8"][j], int(ts_flat[j]))
                     for j in range(N_TS)
                     if (m["ts_int8"][j] is None
                         or m["ts_int8"][j] != int(ts_flat[j]))]
        ts_ok = (len(ts_diffs) == 0)

        # INFERENCE reproducibility (label AND confidence must match Python)
        if m["inference"] is None:
            inf_str = "no INFERENCE"
            inf_ok  = False
        else:
            ml, mc = m["inference"]
            inf_ok = (ml == r["label"]) and (mc == r["conf"])
            inf_str = (f"MCU={ml},{mc} PY={r['label']},{r['conf']}"
                       if not inf_ok else "match")

        if not (stat_ok and ts_ok and inf_ok):
            all_ok = False

        s_cell = f"{len(stat_diffs)}/{N_STAT} diff" if not stat_ok else "OK"
        t_cell = f"{len(ts_diffs)}/{N_TS} diff"    if not ts_ok   else "OK"
        i_cell = inf_str
        print(f"  {i:>3}  {s_cell:>10}  {t_cell:>10}  {i_cell:>14}")

        # Detail rows for first few diffs
        for j, mc, py_v in stat_diffs[:3]:
            mc_str = "<missing>" if mc is None else f"{mc:+4d}"
            print(f"        STAT[{j:2d}] MCU={mc_str}  PY={py_v:+4d}")
        if len(stat_diffs) > 3:
            print(f"        ... and {len(stat_diffs) - 3} more STAT diffs")
        for j, mc, py_v in ts_diffs[:3]:
            mc_str = "<missing>" if mc is None else f"{mc:+4d}"
            print(f"        TS  [{j:3d}] MCU={mc_str}  PY={py_v:+4d}")
        if len(ts_diffs) > 3:
            print(f"        ... and {len(ts_diffs) - 3} more TS diffs")

    print()
    print("=" * 68)
    if all_ok:
        print("  RESULT: MCU reproduces Python reference exactly ✅")
    else:
        print("  RESULT: MCU vs Python diverges ❌")
    print("=" * 68)

    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
