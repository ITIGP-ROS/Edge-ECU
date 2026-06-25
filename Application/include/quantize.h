#pragma once

/**
 * @file    quantize.h
 * @brief   Float32 -> int8 quantization for CubeAI inference inputs.
 *
 * Implements the standard TFLite affine quantization:
 *     q = clamp( round(f / scale) + zero_point, -128, +127 )
 *
 * Two tensors are produced per inference:
 *   - Time-series input  : 50 x 6 float32 -> 300 int8 (row-major / NHWC)
 *   - Statistical input  : 50    float32 -> 50  int8
 *
 * The statistical features must be normalized (z-scored against the training
 * set mean/std stored in stat_norm.h) BEFORE quantization. Call sequence:
 *
 *     Features_Extract(scaled, feats);
 *     Quantize_NormalizeStat(feats);     // in-place z-score
 *     Quantize_Stat(feats, q_stat);      // float -> int8
 *     Quantize_TS  (scaled, q_ts);       // float -> int8
 *
 * MISRA-C:2012 compliant, bare-metal, no malloc.
 */

#include <stdint.h>
#include "norm_params.h"

/*----------------------------------------------------------------------------*/
/*  float32_t — provided by arm_math.h on target, plain typedef on host.      */
/*----------------------------------------------------------------------------*/
#ifdef HOST_BUILD
typedef float float32_t;
#else
#include "arm_math.h"
#endif

/*----------------------------------------------------------------------------*/
/*  Public API                                                                */
/*----------------------------------------------------------------------------*/

/**
 * @brief  Quantize the 50x6 scaled time-series window to int8.
 *
 * Output layout is row-major (NHWC): out_int8[t * N_FEATURES + ch].
 * Uses TS_SCALE / TS_ZP from norm_params.h.
 *
 * @param[in]   scaled    Scaled float32 window  [WINDOW_SIZE][N_FEATURES].
 * @param[out]  out_int8  Quantized output       [WINDOW_SIZE * N_FEATURES].
 */
void Quantize_TS(const float32_t scaled  [WINDOW_SIZE][N_FEATURES],
                       int8_t    out_int8[WINDOW_SIZE * N_FEATURES]);

/**
 * @brief  Quantize the 50-element statistical feature vector to int8.
 *
 * Uses STAT_SCALE / STAT_ZP from norm_params.h.
 * The input is expected to be already z-score normalized
 * (see Quantize_NormalizeStat()).
 *
 * @param[in]   features  Normalized float32 features [N_STAT_FEATURES].
 * @param[out]  out_int8  Quantized output            [N_STAT_FEATURES].
 */
void Quantize_Stat(const float32_t features[N_STAT_FEATURES],
                         int8_t    out_int8[N_STAT_FEATURES]);

/**
 * @brief  Normalize the statistical feature vector in place (z-score).
 *
 * Applies, for each i:
 *     features[i] = (features[i] - stat_mean[i]) / stat_std[i]
 *
 * Where stat_mean[] and stat_std[] are provided by stat_norm.h and originate
 * from the training-set scaler (StandardScaler in the Python pipeline).
 *
 * @param[in,out]  features  Feature vector [N_STAT_FEATURES].
 */
void Quantize_NormalizeStat(float32_t features[N_STAT_FEATURES]);
