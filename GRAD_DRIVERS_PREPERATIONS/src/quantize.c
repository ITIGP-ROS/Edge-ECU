/**
 * @file    quantize.c
 * @brief   Implementation of float32 -> int8 affine quantization for CubeAI.
 *
 * MISRA-C:2012 compliant. No globals, no malloc.
 */

#include <stdint.h>
#include <math.h>
#ifndef HOST_BUILD
#include "arm_math.h"
#endif         /* ADD: replaces local typedef */
#include "norm_params.h"
#include "stat_norm.h"
#include "quantize.h"

/*----------------------------------------------------------------------------*/
/*  Local constants                                                           */
/*----------------------------------------------------------------------------*/
#define INT8_MIN_F   (-128.0f)
#define INT8_MAX_F   ( 127.0f)
#define STD_EPS      (1.0e-10f)   /* guard against /0 in z-score */

/*----------------------------------------------------------------------------*/
/*  Static helpers                                                            */
/*----------------------------------------------------------------------------*/

/**
 * @brief  Single-sample affine quantization with saturating clamp.
 *
 * q = round(f * inv_scale) + zero_point, clamped to [-128, +127].
 * Clamping is performed in float BEFORE the cast — this keeps the
 * conversion well-defined per C11 6.3.1.4 (out-of-range float-to-int
 * is otherwise undefined behaviour).
 */
static inline int8_t quantize_one(float32_t  f,
                                  float32_t  inv_scale,
                                  int32_t    zero_point)
{
    const float32_t scaled = f * inv_scale;
    const float32_t rnd    = nearbyintf(scaled);   /* CHANGE: was manual +/- 0.5 */

    int32_t q = (int32_t)rnd + zero_point;

    if (q < (int32_t)INT8_MIN) { q = (int32_t)INT8_MIN; }   /* CHANGE: use stdint */
    if (q > (int32_t)INT8_MAX) { q = (int32_t)INT8_MAX; }

    return (int8_t)q;
}

/*----------------------------------------------------------------------------*/
/*  Public API                                                                */
/*----------------------------------------------------------------------------*/

void Quantize_TS(const float32_t scaled  [WINDOW_SIZE][N_FEATURES],
                       int8_t    out_int8[WINDOW_SIZE * N_FEATURES])
{
    /* Precompute reciprocal once: VMUL is single-cycle on M4F, VDIV ~14 cyc. */
    const float32_t inv_scale = 1.0f / (float32_t)TS_SCALE;
    const int32_t   zp        = (int32_t)TS_ZP;

    for (uint32_t t = 0U; t < (uint32_t)WINDOW_SIZE; ++t)
    {
        /* Row-major / NHWC layout: out[t*N_FEATURES + ch]. */
        const uint32_t base = t * (uint32_t)N_FEATURES;

        out_int8[base + 0U] = quantize_one(scaled[t][0], inv_scale, zp);
        out_int8[base + 1U] = quantize_one(scaled[t][1], inv_scale, zp);
        out_int8[base + 2U] = quantize_one(scaled[t][2], inv_scale, zp);
        out_int8[base + 3U] = quantize_one(scaled[t][3], inv_scale, zp);
        out_int8[base + 4U] = quantize_one(scaled[t][4], inv_scale, zp);
        out_int8[base + 5U] = quantize_one(scaled[t][5], inv_scale, zp);
    }
}

void Quantize_Stat(const float32_t features[N_STAT_FEATURES],
                         int8_t    out_int8[N_STAT_FEATURES])
{
    const float32_t inv_scale = 1.0f / (float32_t)STAT_SCALE;
    const int32_t   zp        = (int32_t)STAT_ZP;

    for (uint32_t i = 0U; i < (uint32_t)N_STAT_FEATURES; ++i)
    {
        out_int8[i] = quantize_one(features[i], inv_scale, zp);
    }
}

void Quantize_NormalizeStat(float32_t features[N_STAT_FEATURES])
{
    for (uint32_t i = 0U; i < (uint32_t)N_STAT_FEATURES; ++i)
    {
        float32_t s = stat_std[i];
        if (s < STD_EPS)
        {
            s = 1.0f;   /* CHANGE: match Python's behavior */
        }
        features[i] = (features[i] - stat_mean[i]) / s;
    }
}
