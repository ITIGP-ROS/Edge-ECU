#pragma once

/**
 * @file    stat_norm.h
 * @brief   Per-feature mean/std from the training-set StandardScaler.
 *
 * Used by Quantize_NormalizeStat() to z-score the 50-element feature vector
 * before int8 quantization.
 */

#include <stdint.h>

#define N_STAT 50

/* Declarations only — definitions live in stat_norm.c. */
extern const float stat_mean[N_STAT];
extern const float stat_std [N_STAT];
