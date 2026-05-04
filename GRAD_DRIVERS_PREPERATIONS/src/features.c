/**
 * @file    features.c
 * @brief   Implementation of Features_Extract() — must match Python
 *          extract_features() byte-for-byte (within float epsilon).
 *
 * Each feature block is annotated with the equivalent Python line.
 *
 * MISRA-C:2012 compliant. No globals modified outside Features_Init().
 */

#include <stdint.h>
#include <string.h>
#include <math.h>
#include "arm_math.h"
#include "norm_params.h"
#include "features.h"

/*----------------------------------------------------------------------------*/
/*  Local constants                                                           */
/*----------------------------------------------------------------------------*/
#define HALF_WINDOW         (WINDOW_SIZE / 2U)   /* 25 */
#define FFT_NBINS           ((FFT_N / 2U) + 1U)  /* 33 */
#define EPS_STAB            (1.0e-10f)           /* stabiliser, matches Py */
#define POSINF_REPLACE      (10.0f)
#define NEGINF_REPLACE      (-10.0f)

/*----------------------------------------------------------------------------*/
/*  Module-private state                                                      */
/*----------------------------------------------------------------------------*/
/* CMSIS-DSP real-FFT instance, initialised once by Features_Init().         */
static arm_rfft_fast_instance_f32 s_fft_inst;
static uint8_t                    s_fft_ready = 0U;

/*----------------------------------------------------------------------------*/
/*  Forward declarations of static helpers                                    */
/*----------------------------------------------------------------------------*/
static float32_t feat_mean   (const float32_t *x, uint32_t n);
static float32_t feat_var    (const float32_t *x, uint32_t n);
static float32_t feat_std    (const float32_t *x, uint32_t n);
static float32_t feat_mad    (const float32_t *x, uint32_t n);
static float32_t feat_p2p    (const float32_t *x, uint32_t n);
static float32_t feat_rms    (const float32_t *x, uint32_t n);
static float32_t feat_iqr    (const float32_t *x, uint32_t n);
static float32_t feat_skewness(const float32_t *x, uint32_t n);
static float32_t feat_corrcoef(const float32_t *x, const float32_t *y,
                               uint32_t n);

static void      feat_insertion_sort(float32_t *a, uint32_t n);
static float32_t feat_percentile_sorted(const float32_t *sorted_x,
                                        uint32_t n, float32_t p);
static float32_t feat_hfe_channel(const float32_t scaled[WINDOW_SIZE][N_FEATURES],
                                  uint32_t ch);
static void      feat_sanitize(float32_t *v, uint32_t n);

/*----------------------------------------------------------------------------*/
/*  Public API                                                                */
/*----------------------------------------------------------------------------*/

void Features_Init(void)
{
    /* Initialise the 64-point real-FFT instance once. */
    (void)arm_rfft_fast_init_f32(&s_fft_inst, (uint16_t)FFT_N);
    s_fft_ready = 1U;
}

void Features_Extract(const float32_t scaled       [WINDOW_SIZE][N_FEATURES],
                            float32_t features_out [N_STAT_FEATURES])
{
    /* Stack-local working buffers — total ≈ 8 * 50 * 4B = 1.6 KB.
     * (Slightly above the 1 KB hint, but unavoidable: we need per-channel
     * de-interleaved columns + amag + gmag. All other helpers reuse these.)
     * If the budget is hard, channels can be processed one at a time and
     * the column buffer reused — see note at end of file.                   */
    float32_t col[N_FEATURES][WINDOW_SIZE];
    float32_t amag[WINDOW_SIZE];
    float32_t gmag[WINDOW_SIZE];

    uint32_t t = 0U;
    uint32_t ch = 0U;

    /* Guard: refuse to run with an uninitialised FFT instance. Prevents
     * silently producing garbage HFE values if Features_Init() was missed. */
    if (s_fft_ready == 0U)
    {
        return;
    }

    /* ------------------------------------------------------------------ */
    /* 1) De-interleave the window into per-channel columns + compute     */
    /*    amag / gmag in the same pass.                                   */
    /*    Python equivalent:                                              */
    /*      amag = np.sqrt(ax**2 + ay**2 + az**2)                         */
    /*      gmag = np.sqrt(gx**2 + gy**2 + gz**2)                         */
    /* ------------------------------------------------------------------ */
    for (t = 0U; t < (uint32_t)WINDOW_SIZE; ++t)
    {
        const float32_t ax = scaled[t][0];
        const float32_t ay = scaled[t][1];
        const float32_t az = scaled[t][2];
        const float32_t gx = scaled[t][3];
        const float32_t gy = scaled[t][4];
        const float32_t gz = scaled[t][5];

        col[0][t] = ax;
        col[1][t] = ay;
        col[2][t] = az;
        col[3][t] = gx;
        col[4][t] = gy;
        col[5][t] = gz;

        amag[t] = sqrtf((ax * ax) + (ay * ay) + (az * az));
        gmag[t] = sqrtf((gx * gx) + (gy * gy) + (gz * gz));
    }

    /* ------------------------------------------------------------------ */
    /* 2) Per-channel features 0..35                                      */
    /*    Python equivalent (per channel):                                */
    /*      [std, mean(|diff|), ptp, hfe, iqr, rms]                       */
    /* ------------------------------------------------------------------ */
    for (ch = 0U; ch < (uint32_t)N_FEATURES; ++ch)
    {
        const uint32_t base = 6U * ch;

        features_out[base + 0U] = feat_std(col[ch], (uint32_t)WINDOW_SIZE);
        features_out[base + 1U] = feat_mad(col[ch], (uint32_t)WINDOW_SIZE);
        features_out[base + 2U] = feat_p2p(col[ch], (uint32_t)WINDOW_SIZE);
        features_out[base + 3U] = feat_hfe_channel(scaled, ch);
        features_out[base + 4U] = feat_iqr(col[ch], (uint32_t)WINDOW_SIZE);
        features_out[base + 5U] = feat_rms(col[ch], (uint32_t)WINDOW_SIZE);
    }

    /* ------------------------------------------------------------------ */
    /* 3) Magnitude features 36..43                                       */
    /*    Python:                                                         */
    /*      [std(amag), mean(|diff(amag)|), ptp(amag), rms(amag),         */
    /*       std(gmag), mean(|diff(gmag)|), ptp(gmag), rms(gmag)]         */
    /* ------------------------------------------------------------------ */
    features_out[36] = feat_std(amag, (uint32_t)WINDOW_SIZE);
    features_out[37] = feat_mad(amag, (uint32_t)WINDOW_SIZE);
    features_out[38] = feat_p2p(amag, (uint32_t)WINDOW_SIZE);
    features_out[39] = feat_rms(amag, (uint32_t)WINDOW_SIZE);

    features_out[40] = feat_std(gmag, (uint32_t)WINDOW_SIZE);
    features_out[41] = feat_mad(gmag, (uint32_t)WINDOW_SIZE);
    features_out[42] = feat_p2p(gmag, (uint32_t)WINDOW_SIZE);
    features_out[43] = feat_rms(gmag, (uint32_t)WINDOW_SIZE);

    /* ------------------------------------------------------------------ */
    /* 4) Correlations 44..45                                             */
    /*    Python:                                                         */
    /*      np.corrcoef(amag, gmag)[0,1]                                  */
    /*      np.corrcoef(az,   gz  )[0,1]                                  */
    /* ------------------------------------------------------------------ */
    features_out[44] = feat_corrcoef(amag, gmag, (uint32_t)WINDOW_SIZE);
    features_out[45] = feat_corrcoef(col[2], col[5], (uint32_t)WINDOW_SIZE);

    /* ------------------------------------------------------------------ */
    /* 5) Skewness 46..47 — for az and gz                                 */
    /*    Python:                                                         */
    /*      m = mean(x); s = std(x)                                       */
    /*      skew = mean(((x - m) / s)**3)  if s >= 1e-10 else 0           */
    /* ------------------------------------------------------------------ */
    features_out[46] = feat_skewness(col[2], (uint32_t)WINDOW_SIZE);
    features_out[47] = feat_skewness(col[5], (uint32_t)WINDOW_SIZE);

    /* ------------------------------------------------------------------ */
    /* 6) Variance ratios 48..49 (second half / first half)               */
    /*    Python:                                                         */
    /*      half = 25                                                     */
    /*      (var(amag[half:]) + 1e-10) / (var(amag[:half]) + 1e-10)       */
    /* ------------------------------------------------------------------ */
    {
        const float32_t var_a_lo = feat_var(&amag[0],          HALF_WINDOW);
        const float32_t var_a_hi = feat_var(&amag[HALF_WINDOW], HALF_WINDOW);
        const float32_t var_g_lo = feat_var(&gmag[0],          HALF_WINDOW);
        const float32_t var_g_hi = feat_var(&gmag[HALF_WINDOW], HALF_WINDOW);

        features_out[48] = (var_a_hi + EPS_STAB) / (var_a_lo + EPS_STAB);
        features_out[49] = (var_g_hi + EPS_STAB) / (var_g_lo + EPS_STAB);
    }

    /* ------------------------------------------------------------------ */
    /* 7) NaN/Inf sweep                                                   */
    /*    Python: np.nan_to_num(f, nan=0, posinf=10, neginf=-10)          */
    /* ------------------------------------------------------------------ */
    feat_sanitize(features_out, (uint32_t)N_STAT_FEATURES);
}

/*============================================================================*/
/*  Static helpers                                                            */
/*============================================================================*/

/* ---------- mean / var / std (population) -------------------------------- */

static float32_t feat_mean(const float32_t *x, uint32_t n)
{
    if (n == 0U)
    {
        return 0.0f;
    }

    float32_t s = 0.0f;
    for (uint32_t i = 0U; i < n; ++i)
    {
        s += x[i];
    }
    return s / (float32_t)n;
}

static float32_t feat_var(const float32_t *x, uint32_t n)
{
    if (n == 0U)
    {
        return 0.0f;
    }

    /* Population variance: sum((x - m)^2) / N — matches numpy default. */
    const float32_t m = feat_mean(x, n);
    float32_t       s = 0.0f;
    for (uint32_t i = 0U; i < n; ++i)
    {
        const float32_t d = x[i] - m;
        s += d * d;
    }
    return s / (float32_t)n;
}

static float32_t feat_std(const float32_t *x, uint32_t n)
{
    return sqrtf(feat_var(x, n));
}

/* ---------- mean(|diff(x)|) ---------------------------------------------- */

static float32_t feat_mad(const float32_t *x, uint32_t n)
{
    /* Python: np.mean(np.abs(np.diff(x))) — diff length = n-1. */
    if (n < 2U)
    {
        return 0.0f;
    }
    float32_t s = 0.0f;
    for (uint32_t i = 1U; i < n; ++i)
    {
        s += fabsf(x[i] - x[i - 1U]);
    }
    return s / (float32_t)(n - 1U);
}

/* ---------- peak-to-peak ------------------------------------------------- */

static float32_t feat_p2p(const float32_t *x, uint32_t n)
{
    if (n == 0U)
    {
        return 0.0f;
    }

    float32_t mn = x[0];
    float32_t mx = x[0];
    for (uint32_t i = 1U; i < n; ++i)
    {
        if (x[i] < mn) { mn = x[i]; }
        if (x[i] > mx) { mx = x[i]; }
    }
    return mx - mn;
}

/* ---------- root-mean-square --------------------------------------------- */

static float32_t feat_rms(const float32_t *x, uint32_t n)
{
    if (n == 0U)
    {
        return 0.0f;
    }

    float32_t s = 0.0f;
    for (uint32_t i = 0U; i < n; ++i)
    {
        s += x[i] * x[i];
    }
    return sqrtf(s / (float32_t)n);
}

/* ---------- IQR (numpy.percentile linear-interp default) ----------------- */

static void feat_insertion_sort(float32_t *a, uint32_t n)
{
    for (uint32_t i = 1U; i < n; ++i)
    {
        const float32_t key = a[i];
        uint32_t        j   = i;
        while ((j > 0U) && (a[j - 1U] > key))
        {
            a[j] = a[j - 1U];
            --j;
        }
        a[j] = key;
    }
}

static float32_t feat_percentile_sorted(const float32_t *sorted_x,
                                        uint32_t n, float32_t p)
{
    /* Linear-interpolation percentile (numpy default).
     *   idx  = (p/100) * (n - 1)
     *   lo   = floor(idx); hi = ceil(idx)
     *   frac = idx - lo
     *   val  = sorted[lo] + frac * (sorted[hi] - sorted[lo])               */
    const float32_t idx  = (p * 0.01f) * (float32_t)(n - 1U);
    const float32_t flo  = floorf(idx);
    const uint32_t  lo   = (uint32_t)flo;
    uint32_t        hi   = lo + 1U;
    if (hi >= n) { hi = n - 1U; }
    const float32_t frac = idx - flo;
    return sorted_x[lo] + (frac * (sorted_x[hi] - sorted_x[lo]));
}

static float32_t feat_iqr(const float32_t *x, uint32_t n)
{
    /* Defensive cap: clamp n so the stack-local copy can never overflow. */
    if (n > (uint32_t)WINDOW_SIZE)
    {
        n = (uint32_t)WINDOW_SIZE;
    }

    /* Stack-local copy — input is not modified. WINDOW_SIZE = 50 → 200 B. */
    float32_t buf[WINDOW_SIZE];
    for (uint32_t i = 0U; i < n; ++i)
    {
        buf[i] = x[i];
    }
    feat_insertion_sort(buf, n);

    const float32_t q1 = feat_percentile_sorted(buf, n, 25.0f);
    const float32_t q3 = feat_percentile_sorted(buf, n, 75.0f);
    return q3 - q1;
}

/* ---------- skewness ----------------------------------------------------- */

static float32_t feat_skewness(const float32_t *x, uint32_t n)
{
    if (n == 0U)
    {
        return 0.0f;
    }

    const float32_t m = feat_mean(x, n);

    /* Population std */
    float32_t var_acc = 0.0f;
    for (uint32_t i = 0U; i < n; ++i)
    {
        const float32_t d = x[i] - m;
        var_acc += d * d;
    }
    const float32_t s = sqrtf(var_acc / (float32_t)n);

    if (s < EPS_STAB)
    {
        return 0.0f;
    }

    const float32_t inv_s = 1.0f / s;
    float32_t       acc   = 0.0f;
    for (uint32_t i = 0U; i < n; ++i)
    {
        const float32_t z = (x[i] - m) * inv_s;
        acc += z * z * z;
    }
    return acc / (float32_t)n;
}

/* ---------- Pearson correlation coefficient ------------------------------ */

static float32_t feat_corrcoef(const float32_t *x, const float32_t *y,
                               uint32_t n)
{
    if (n == 0U)
    {
        return 0.0f;
    }

    /* Python: np.corrcoef(x, y)[0,1].
     *   if std(x) < 1e-10 or std(y) < 1e-10 → 0.0
     *   else cov(x,y) / (std(x) * std(y))                                  */
    const float32_t mx = feat_mean(x, n);
    const float32_t my = feat_mean(y, n);

    float32_t sxx = 0.0f;
    float32_t syy = 0.0f;
    float32_t sxy = 0.0f;
    for (uint32_t i = 0U; i < n; ++i)
    {
        const float32_t dx = x[i] - mx;
        const float32_t dy = y[i] - my;
        sxx += dx * dx;
        syy += dy * dy;
        sxy += dx * dy;
    }

    const float32_t inv_n = 1.0f / (float32_t)n;
    const float32_t std_x = sqrtf(sxx * inv_n);
    const float32_t std_y = sqrtf(syy * inv_n);

    if ((std_x < EPS_STAB) || (std_y < EPS_STAB))
    {
        return 0.0f;
    }

    const float32_t cov = sxy * inv_n;
    return cov / (std_x * std_y);
}

/* ---------- HFE: high-frequency energy ratio (FFT-based) ----------------- */

static float32_t feat_hfe_channel(const float32_t scaled[WINDOW_SIZE][N_FEATURES],
                                  uint32_t ch)
{
    /* Static so the 64-float in/out buffers don't blow the stack on each
     * call. They are only written before being read inside this function,
     * so there is no cross-call aliasing concern provided the module is
     * not called re-entrantly (single inference task — see header). */
    static float32_t fft_in [FFT_N]      __attribute__((aligned(4)));
    static float32_t fft_out[FFT_N]      __attribute__((aligned(4)));
    static float32_t mag_sq [FFT_NBINS];

    uint32_t i;

    /* Zero-pad: copy 50 samples into [0..49], zero [50..63]. */
    for (i = 0U; i < (uint32_t)WINDOW_SIZE; ++i)
    {
        fft_in[i] = scaled[i][ch];
    }
    for (i = (uint32_t)WINDOW_SIZE; i < (uint32_t)FFT_N; ++i)
    {
        fft_in[i] = 0.0f;
    }

    /* Forward 64-point real FFT (CMSIS-DSP packed format). */
    arm_rfft_fast_f32(&s_fft_inst, fft_in, fft_out, 0U);

    /* CMSIS-DSP packing:
     *   fft_out[0]    = DC   (real)
     *   fft_out[1]    = Nyq  (real)   <-- packed in slot of imag-of-DC
     *   fft_out[2k], fft_out[2k+1] = (real, imag) of bin k for k = 1..31
     *
     * numpy.fft.rfft returns N/2+1 = 33 complex bins; we reconstruct |X|^2
     * for each of those 33 bins in mag_sq[0..32].                            */
    mag_sq[0]  = fft_out[0] * fft_out[0];          /* DC      */
    mag_sq[32] = fft_out[1] * fft_out[1];          /* Nyquist */
    for (i = 1U; i < 32U; ++i)
    {
        const float32_t re = fft_out[2U * i];
        const float32_t im = fft_out[(2U * i) + 1U];
        mag_sq[i] = (re * re) + (im * im);
    }

    /* Total energy + 1e-10 stabiliser, and high-freq energy from bin 16. */
    float32_t total   = EPS_STAB;
    float32_t hfe_sum = 0.0f;
    for (i = 0U; i < FFT_NBINS; ++i)
    {
        total += mag_sq[i];
    }
    for (i = (uint32_t)FFT_HF_SPLIT; i < FFT_NBINS; ++i)
    {
        hfe_sum += mag_sq[i];
    }

    return hfe_sum / total;
}

/* ---------- NaN/Inf sanitiser -------------------------------------------- */

static void feat_sanitize(float32_t *v, uint32_t n)
{
    for (uint32_t i = 0U; i < n; ++i)
    {
        if (isnan(v[i]))
        {
            v[i] = 0.0f;
        }
        else if (isinf(v[i]))
        {
            v[i] = (v[i] > 0.0f) ? POSINF_REPLACE : NEGINF_REPLACE;
        }
        else
        {
            /* finite — leave as-is */
        }
    }
}
