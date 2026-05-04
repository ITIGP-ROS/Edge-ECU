#!/usr/bin/env python3
"""
tools/mcu_compare.py — Compare an MCU verification dump against the
                       expected reference values in tools/test_vectors.h.

Multi-block: handles all N_TEST_VECTORS blocks in a single dump.

Usage:
    cd tools/
    python3 mcu_compare.py [mcu_dump.txt]

Exit codes:
    0 — all vectors passed all comparisons
    1 — at least one comparison failure (FEATURES / TS_INT8 / STAT_INT8)
    2 — parse error or MCU buffer overflow detected (!OF sentinel)
"""

import re
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# ANSI color codes
# ---------------------------------------------------------------------------
GREEN  = "\033[32m"
RED    = "\033[31m"
YELLOW = "\033[33m"
BOLD   = "\033[1m"
DIM    = "\033[2m"
RESET  = "\033[0m"

# ---------------------------------------------------------------------------
# Tolerances and config
# ---------------------------------------------------------------------------
FEATURES_ATOL = 1e-3
FEATURES_RTOL = 1e-3

N_TEST_VECTORS    = 5
N_FEATURES_TOTAL  = 50    # stat feature vector length
N_TS_TOTAL        = 300   # WINDOW_SIZE * N_FEATURES = 50 * 6

# If True, ground-truth label mismatch (model misprediction) counts as a
# failure. Default False — model accuracy is not the same thing as
# preprocessing correctness.
STRICT_LABEL_CHECK = False

# ---------------------------------------------------------------------------
# Feature names (for nicer failure reports)
# ---------------------------------------------------------------------------
def _build_feature_names():
    names = []
    feats = ["std", "mad", "p2p", "hfe", "iqr", "rms"]
    chans = ["ax", "ay", "az", "gx", "gy", "gz"]
    for ch in chans:
        for f in feats:
            names.append(f"{f}_{ch}")
    for stat in ["std", "mad", "p2p", "rms"]:
        names.append(f"{stat}_amag")
    for stat in ["std", "mad", "p2p", "rms"]:
        names.append(f"{stat}_gmag")
    names.append("corr_amag_gmag")
    names.append("corr_az_gz")
    names.append("skew_az")
    names.append("skew_gz")
    names.append("varratio_amag")
    names.append("varratio_gmag")
    return names

FEATURE_NAMES = _build_feature_names()
assert len(FEATURE_NAMES) == N_FEATURES_TOTAL


# ---------------------------------------------------------------------------
# Dump parser — multi-block
# ---------------------------------------------------------------------------
def _new_block(vec_idx, label):
    return {
        "vec":        vec_idx,
        "label":      label,         # ground-truth from VERIFY_START marker
        "features":   [None] * N_FEATURES_TOTAL,
        "stat_int8":  [None] * N_FEATURES_TOTAL,
        "ts_int8":    [None] * N_TS_TOTAL,
        "inference":  None,          # (predicted_label, confidence) or None
    }


def parse_dump(text):
    """Parse the MCU UART dump into a list of per-vector blocks.

    Each block dict carries: vec, label, features, stat_int8, ts_int8,
    inference. Lines outside any block are ignored. Blocks are appended
    on ===VERIFY_END===; an unterminated final block is dropped with a
    warning.

    Raises SystemExit(2) on !OF sentinel anywhere in the dump.
    """
    blocks = []
    cur    = None

    start_re = re.compile(r"===VERIFY_START===,VEC=(\d+),LABEL=(\d+)")

    for raw_line in text.splitlines():
        # Strip stray null/0xff/control bytes that some terminals introduce
        line = raw_line.lstrip("\x00\xff\r\n\t ")

        m = start_re.search(line)
        if m:
            if cur is not None:
                # Unterminated previous block — keep what we have but warn
                print(f"{YELLOW}WARN: VERIFY_START seen before previous "
                      f"VERIFY_END (vec={cur['vec']}); keeping partial "
                      f"block.{RESET}", file=sys.stderr)
                blocks.append(cur)
            cur = _new_block(int(m.group(1)), int(m.group(2)))
            continue

        if "===VERIFY_END===" in line:
            if cur is not None:
                blocks.append(cur)
                cur = None
            continue

        if cur is None:
            continue   # line outside any block

        if "!OF" in line:
            print(
                f"{RED}ERROR: buffer overflow sentinel detected in vec="
                f"{cur['vec']} line:{RESET}\n  {line[:120]}",
                file=sys.stderr,
            )
            sys.exit(2)

        m = re.match(r"FEATURES_(\d+),(.+)", line)
        if m:
            idx = int(m.group(1))
            try:
                vals = [float(x) for x in m.group(2).split(",")]
            except ValueError as e:
                print(f"{RED}ERROR: bad float in vec={cur['vec']}: "
                      f"{line[:80]}: {e}{RESET}", file=sys.stderr)
                sys.exit(2)
            start = idx * 25
            for i, v in enumerate(vals):
                if start + i < N_FEATURES_TOTAL:
                    cur["features"][start + i] = v
            continue

        m = re.match(r"STAT_INT8,(.+)", line)
        if m:
            try:
                vals = [int(x) for x in m.group(1).split(",")]
            except ValueError as e:
                print(f"{RED}ERROR: bad int in vec={cur['vec']}: "
                      f"{line[:80]}: {e}{RESET}", file=sys.stderr)
                sys.exit(2)
            for i, v in enumerate(vals):
                if i < N_FEATURES_TOTAL:
                    cur["stat_int8"][i] = v
            continue

        m = re.match(r"TS_INT8_(\d+),(.+)", line)
        if m:
            idx = int(m.group(1))
            try:
                vals = [int(x) for x in m.group(2).split(",")]
            except ValueError as e:
                print(f"{RED}ERROR: bad int in vec={cur['vec']}: "
                      f"{line[:80]}: {e}{RESET}", file=sys.stderr)
                sys.exit(2)
            start = idx * 100
            for i, v in enumerate(vals):
                if start + i < N_TS_TOTAL:
                    cur["ts_int8"][start + i] = v
            continue

        m = re.match(r"INFERENCE,(\d+),(\d+)", line)
        if m:
            cur["inference"] = (int(m.group(1)), int(m.group(2)))
            continue

    if cur is not None:
        print(f"{YELLOW}WARN: dump ended without ===VERIFY_END=== for "
              f"vec={cur['vec']}; dropping partial block.{RESET}",
              file=sys.stderr)

    return blocks


# ---------------------------------------------------------------------------
# test_vectors.h parser — extract ALL top-level inner blocks
# ---------------------------------------------------------------------------
def _strip_c_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//[^\n]*", "", text)
    return text


def _extract_all_inner_blocks(body):
    """Return a list of strings, each being the contents of one balanced
    top-level `{...}` group inside `body`. Used to walk
    `expected_xxx[N_TEST_VECTORS][...]` initializers.
    """
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


def _tokenize_numbers(inner):
    flat = inner.replace("{", " ").replace("}", " ")
    tokens = []
    for tok in flat.split(","):
        tok = tok.strip()
        if not tok:
            continue
        tok = tok.rstrip("fFuUlL")
        tokens.append(tok)
    return tokens


def parse_vector_h_all(path, var_name):
    """Locate `var_name = { ... };` and return a list of token-string lists,
    one per top-level inner block (i.e. one per test vector).

    Returns None if the variable is not declared in the file.
    """
    raw  = Path(path).read_text()
    text = _strip_c_comments(raw)

    pattern = re.compile(
        rf"\b{re.escape(var_name)}\b[^=;{{]*=\s*\{{(.*?)\}}\s*;",
        re.DOTALL,
    )
    m = pattern.search(text)
    if not m:
        return None

    body   = m.group(1)
    inners = _extract_all_inner_blocks(body)
    if not inners:
        # 1D fallback (shouldn't happen for our [N_TEST_VECTORS][...] arrays)
        return [_tokenize_numbers(body)]

    return [_tokenize_numbers(inn) for inn in inners]


def load_expected(path, n_vectors):
    """Return a list of n_vectors dicts, each with keys:
       'features' (50 floats), 'ts_int8' (300 ints),
       'stat_int8' (50 ints) or None if optional reference absent.
    """
    feats_all = parse_vector_h_all(path, "expected_features")
    if feats_all is None:
        print(f"{RED}ERROR: expected_features not found in {path}{RESET}",
              file=sys.stderr)
        sys.exit(2)
    if len(feats_all) < n_vectors:
        print(f"{RED}ERROR: expected_features has {len(feats_all)} "
              f"vectors, need {n_vectors}{RESET}", file=sys.stderr)
        sys.exit(2)

    ts_all = parse_vector_h_all(path, "expected_ts_int8")
    if ts_all is None:
        print(f"{RED}ERROR: expected_ts_int8 not found in {path}{RESET}",
              file=sys.stderr)
        sys.exit(2)
    if len(ts_all) < n_vectors:
        print(f"{RED}ERROR: expected_ts_int8 has {len(ts_all)} vectors, "
              f"need {n_vectors}{RESET}", file=sys.stderr)
        sys.exit(2)

    stat_all = parse_vector_h_all(path, "expected_stat_int8")  # optional

    expected = []
    for i in range(n_vectors):
        if len(feats_all[i]) < N_FEATURES_TOTAL:
            print(f"{RED}ERROR: expected_features[{i}] has "
                  f"{len(feats_all[i])} tokens, need {N_FEATURES_TOTAL}"
                  f"{RESET}", file=sys.stderr)
            sys.exit(2)
        if len(ts_all[i]) < N_TS_TOTAL:
            print(f"{RED}ERROR: expected_ts_int8[{i}] has "
                  f"{len(ts_all[i])} tokens, need {N_TS_TOTAL}{RESET}",
                  file=sys.stderr)
            sys.exit(2)

        entry = {
            "features": [float(t) for t in feats_all[i][:N_FEATURES_TOTAL]],
            "ts_int8":  [int(t)   for t in ts_all[i][:N_TS_TOTAL]],
            "stat_int8": None,
        }
        if (stat_all is not None
                and i < len(stat_all)
                and len(stat_all[i]) >= N_FEATURES_TOTAL):
            entry["stat_int8"] = [int(t) for t in stat_all[i][:N_FEATURES_TOTAL]]
        expected.append(entry)

    return expected


# ---------------------------------------------------------------------------
# Comparators
# ---------------------------------------------------------------------------
def _close(a, b, atol, rtol):
    return abs(a - b) <= (atol + rtol * abs(b))


def compare_features(mcu, py):
    fails = []
    max_abs = 0.0
    for i in range(N_FEATURES_TOTAL):
        if mcu[i] is None:
            fails.append((i, None, py[i], float("inf")))
            continue
        diff = abs(mcu[i] - py[i])
        if diff > max_abs:
            max_abs = diff
        if not _close(mcu[i], py[i], FEATURES_ATOL, FEATURES_RTOL):
            fails.append((i, mcu[i], py[i], diff))
    return fails, max_abs


def compare_int8_array(mcu, py, n):
    fails = []
    for i in range(n):
        if mcu[i] is None or mcu[i] != py[i]:
            fails.append((i, mcu[i], py[i]))
    return fails


# ---------------------------------------------------------------------------
# Reporting
# ---------------------------------------------------------------------------
def banner(title):
    print("=" * 64)
    print(f"  {BOLD}{title}{RESET}")
    print("=" * 64)


def _fmt_status(passed, skipped=False):
    if skipped:
        return f"{YELLOW}SKIP{RESET}"
    return f"{GREEN} OK {RESET}" if passed else f"{RED}FAIL{RESET}"


def report_block(blk, exp):
    """Print a per-vector report. Returns dict of pass flags."""
    vec   = blk["vec"]
    label = blk["label"]

    print(f"\n{BOLD}── Vector {vec}  (label={label}) ──{RESET}")

    # FEATURES
    feat_fails, feat_max = compare_features(blk["features"], exp["features"])
    feat_ok = (len(feat_fails) == 0)
    if feat_ok:
        print(f"  ✅ {GREEN}FEATURES{RESET}    50/50 within tol  "
              f"(max_abs_err = {feat_max:.2e})")
    else:
        print(f"  ❌ {RED}FEATURES{RESET}    "
              f"{len(feat_fails)}/{N_FEATURES_TOTAL} mismatch  "
              f"(max_abs_err = {feat_max:.2e}, tol={FEATURES_ATOL:.0e})")
        for idx, c_val, py_val, diff in feat_fails[:8]:
            name = FEATURE_NAMES[idx]
            if c_val is None:
                print(f"      [{idx:2d}] {name:<18s} "
                      f"C= <missing>  PY= {py_val:+.6f}")
            else:
                print(f"      [{idx:2d}] {name:<18s} "
                      f"C= {c_val:+.6f}  PY= {py_val:+.6f}  "
                      f"diff={diff:.3e}")
        if len(feat_fails) > 8:
            print(f"      ... and {len(feat_fails) - 8} more")

    # TS_INT8
    ts_fails = compare_int8_array(blk["ts_int8"], exp["ts_int8"], N_TS_TOTAL)
    ts_ok = (len(ts_fails) == 0)
    if ts_ok:
        print(f"  ✅ {GREEN}TS_INT8{RESET}     {N_TS_TOTAL}/{N_TS_TOTAL} "
              f"bytes match exactly")
    else:
        print(f"  ❌ {RED}TS_INT8{RESET}     "
              f"{len(ts_fails)}/{N_TS_TOTAL} mismatch")
        for idx, c_val, py_val in ts_fails[:5]:
            t  = idx // 6
            ch = ["ax", "ay", "az", "gx", "gy", "gz"][idx % 6]
            c_str = "<missing>" if c_val is None else f"{c_val:+4d}"
            print(f"      [{idx:3d}] (t={t:2d},{ch}) "
                  f"C= {c_str}  PY= {py_val:+4d}")
        if len(ts_fails) > 5:
            print(f"      ... and {len(ts_fails) - 5} more")

    # STAT_INT8 (optional reference)
    if exp["stat_int8"] is None:
        stat_ok = None  # skipped
        print(f"  ⚠️  {YELLOW}STAT_INT8{RESET}   skipped "
              f"(no Python reference in test_vectors.h)")
    else:
        stat_fails = compare_int8_array(blk["stat_int8"],
                                        exp["stat_int8"],
                                        N_FEATURES_TOTAL)
        stat_ok = (len(stat_fails) == 0)
        if stat_ok:
            print(f"  ✅ {GREEN}STAT_INT8{RESET}   "
                  f"{N_FEATURES_TOTAL}/{N_FEATURES_TOTAL} bytes match exactly")
        else:
            print(f"  ❌ {RED}STAT_INT8{RESET}   "
                  f"{len(stat_fails)}/{N_FEATURES_TOTAL} mismatch")
            for idx, c_val, py_val in stat_fails[:8]:
                name = FEATURE_NAMES[idx]
                c_str = "<missing>" if c_val is None else f"{c_val:+4d}"
                print(f"      [{idx:2d}] {name:<18s} "
                      f"C= {c_str}  PY= {py_val:+4d}")
            if len(stat_fails) > 8:
                print(f"      ... and {len(stat_fails) - 8} more")

    # INFERENCE — sanity vs ground truth (NOT a verification check)
    if blk["inference"] is None:
        pred_ok = None
        print(f"  ⚠️  {YELLOW}INFERENCE{RESET}   no INFERENCE line in dump")
    else:
        pred_label, conf = blk["inference"]
        pred_ok = (pred_label == label)
        tag = (f"{GREEN}match{RESET}" if pred_ok
               else f"{YELLOW}mispredict{RESET}")
        print(f"  ℹ️  {BOLD}INFERENCE{RESET}   "
              f"pred={pred_label}, conf={conf}, gt={label}  [{tag}]"
              f"  {DIM}(model accuracy, not preprocessing){RESET}")

    return {
        "features":  feat_ok,
        "ts_int8":   ts_ok,
        "stat_int8": stat_ok,
        "predict":   pred_ok,
    }


def print_summary(results):
    """Print the 5-row summary table at the end."""
    banner("SUMMARY")
    print(f"  {'VEC':>3}  {'FEATURES':>10}  {'STAT_INT8':>10}  "
          f"{'TS_INT8':>10}  {'PRED':>10}")
    print(f"  {'-'*3}  {'-'*10}  {'-'*10}  {'-'*10}  {'-'*10}")

    for vec_idx, res in sorted(results.items()):
        if res is None:
            print(f"  {vec_idx:>3}  {RED}{'MISSING':>10}{RESET}")
            continue
        cells = []
        for key in ("features", "stat_int8", "ts_int8", "predict"):
            v = res[key]
            if v is None:
                cells.append(f"{YELLOW}{'SKIP':>10}{RESET}")
            elif v:
                cells.append(f"{GREEN}{'OK':>10}{RESET}")
            else:
                cells.append(f"{RED}{'FAIL':>10}{RESET}")
        print(f"  {vec_idx:>3}  " + "  ".join(cells))


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------
def main(argv):
    dump_path = Path(argv[1]) if len(argv) > 1 else Path("mcu_dump.txt")
    vec_path  = Path(__file__).parent / "test_vectors.h"

    if not dump_path.exists():
        print(f"{RED}ERROR: dump file not found: {dump_path}{RESET}",
              file=sys.stderr)
        return 2
    if not vec_path.exists():
        print(f"{RED}ERROR: test_vectors.h not found: {vec_path}{RESET}",
              file=sys.stderr)
        return 2

    try:
        dump_text = dump_path.read_text(errors="replace")
    except OSError as e:
        print(f"{RED}ERROR reading dump: {e}{RESET}", file=sys.stderr)
        return 2

    blocks   = parse_dump(dump_text)
    expected = load_expected(vec_path, N_TEST_VECTORS)

    if not blocks:
        print(f"{RED}ERROR: no VERIFY_START/END blocks found in dump"
              f"{RESET}", file=sys.stderr)
        return 2

    banner("MCU Verification Comparison (multi-block)")
    print(f"  Dump:    {dump_path}")
    print(f"  Vectors: {vec_path}")
    print(f"  Found:   {len(blocks)} block(s) in dump, "
          f"{len(expected)} reference vector(s)")

    # Index dump blocks by vec index for matching against references
    by_vec = {}
    for blk in blocks:
        if blk["vec"] in by_vec:
            print(f"{YELLOW}WARN: duplicate VEC={blk['vec']} in dump; "
                  f"keeping last occurrence.{RESET}", file=sys.stderr)
        by_vec[blk["vec"]] = blk

    results = {}
    overall_pass = True

    for vec_idx in range(N_TEST_VECTORS):
        if vec_idx not in by_vec:
            print(f"\n{BOLD}── Vector {vec_idx} ──{RESET}")
            print(f"  {RED}MISSING from dump{RESET}")
            results[vec_idx] = None
            overall_pass = False
            continue

        res = report_block(by_vec[vec_idx], expected[vec_idx])
        results[vec_idx] = res

        # Fold pass flags into overall
        if res["features"] is False:  noqa = True; overall_pass = False
        if res["ts_int8"]  is False:  overall_pass = False
        if res["stat_int8"] is False: overall_pass = False
        if STRICT_LABEL_CHECK and (res["predict"] is False):
            overall_pass = False

    print()
    print_summary(results)

    print()
    banner("RESULT: " + (f"{GREEN}PASS{RESET}" if overall_pass
                         else f"{RED}FAIL{RESET}"))

    return 0 if overall_pass else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
