#pragma once

/**
 * @file    scale.h
 * @brief   Scaling of raw MPU6050 IMU samples into physical units.
 *
 * Converts a window of raw int16 IMU samples (accelerometer + gyroscope)
 * into float32 values matching the training pipeline of the ML model.
 *
 * MISRA-C:2012 compliant, bare-metal (no HAL, no malloc).
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
 * @brief  Scale a raw IMU window into physical units.
 *
 * Channel layout (per timestep):
 *   [0] = ax, [1] = ay, [2] = az  -> divided by ACCEL_SCALE (8192.0f, ±4g)
 *   [3] = gx, [4] = gy, [5] = gz  -> divided by GYRO_SCALE  (65.5f,  ±500 dps)
 *
 * @param[in]   raw_in       Raw int16 samples, [WINDOW_SIZE][N_FEATURES].
 * @param[out]  scaled_out   Scaled float32 samples, [WINDOW_SIZE][N_FEATURES].
 *
 * @note  No side effects, no globals, no dynamic allocation.
 */
void Scale_RawWindow(const int16_t   raw_in    [WINDOW_SIZE][N_FEATURES],
                           float32_t scaled_out[WINDOW_SIZE][N_FEATURES]);
