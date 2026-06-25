#pragma once

/**
 * @file    features.h
 * @brief   Hand-crafted statistical/spectral feature extraction from a
 *          scaled IMU window.
 *
 * Produces a 50-element feature vector that is bit-compatible (within
 * float epsilon) with the Python training-pipeline extract_features().
 *
 * MISRA-C:2012 compliant, bare-metal, no malloc, no recursion.
 *
 * Usage:
 *     Features_Init();                          // once, from main()
 *     Features_Extract(scaled, feats);          // per inference
 */

#include <stdint.h>
#include "norm_params.h"


/*----------------------------------------------------------------------------*/
/*  Type aliases (kept consistent with project STD_TYPES.h)                   */
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
 * @brief  One-time initialisation of the feature-extraction module.
 *
 * Initialises the CMSIS-DSP 64-point real-FFT instance used by the HFE
 * feature. Must be called once from main() before any call to
 * Features_Extract().
 */
void Features_Init(void);

/**
 * @brief  Extract the 50-element feature vector from a scaled IMU window.
 *
 * Channel layout of input (per timestep):
 *   [0]=ax, [1]=ay, [2]=az, [3]=gx, [4]=gy, [5]=gz   (physical units)
 *
 * Output layout (must match Python extract_features() exactly):
 *   [ 0..35] : 6 features  x 6 channels (std, MAD, P2P, HFE, IQR, RMS)
 *   [36..39] : amag        (std, MAD, P2P, RMS)
 *   [40..43] : gmag        (std, MAD, P2P, RMS)
 *   [44]     : corrcoef(amag, gmag)
 *   [45]     : corrcoef(az, gz)
 *   [46]     : skewness(az)
 *   [47]     : skewness(gz)
 *   [48]     : var(amag[25:]) / var(amag[:25])  (with 1e-10 stabiliser)
 *   [49]     : var(gmag[25:]) / var(gmag[:25])  (with 1e-10 stabiliser)
 *
 * NaN / +Inf / -Inf in the output are replaced by 0.0f / +10.0f / -10.0f
 * (matching numpy.nan_to_num(nan=0, posinf=10, neginf=-10)).
 *
 * @param[in]   scaled        Scaled window [WINDOW_SIZE][N_FEATURES].
 * @param[out]  features_out  Feature vector [N_STAT_FEATURES].
 *
 * @note  This function is NOT re-entrant. It uses module-static buffers
 *        for the FFT computation. Call only from a single thread context.
 */
void Features_Extract(const float32_t scaled       [WINDOW_SIZE][N_FEATURES],
                            float32_t features_out [N_STAT_FEATURES]);
