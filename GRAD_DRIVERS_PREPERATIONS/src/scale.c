/**
 * @file    scale.c
 * @brief   Implementation of raw IMU window scaling.
 *
 * Uses reciprocal multiplication instead of division for performance
 * on the Cortex-M4F FPU (single-cycle VMUL vs. multi-cycle VDIV).
 *
 * MISRA-C:2012 compliant.
 */

#include <stdint.h>
#ifndef HOST_BUILD
#include "arm_math.h"
#endif
#include "norm_params.h"
#include "scale.h"

/*----------------------------------------------------------------------------*/
/*  Public function                                                           */
/*----------------------------------------------------------------------------*/

void Scale_RawWindow(const int16_t   raw_in    [WINDOW_SIZE][N_FEATURES],
                           float32_t scaled_out[WINDOW_SIZE][N_FEATURES])
{
    /* Precompute reciprocals once -> avoids per-sample division.            */
    const float32_t inv_accel_scale = 1.0f / ACCEL_SCALE;  /* 1 / 8192.0f */
    const float32_t inv_gyro_scale  = 1.0f / GYRO_SCALE;   /* 1 / 65.5f   */

    for (uint32_t t = 0U; t < (uint32_t)WINDOW_SIZE; ++t)
    {
        /* Accelerometer channels: 0..2 */
        scaled_out[t][0] = ((float32_t)raw_in[t][0]) * inv_accel_scale;
        scaled_out[t][1] = ((float32_t)raw_in[t][1]) * inv_accel_scale;
        scaled_out[t][2] = ((float32_t)raw_in[t][2]) * inv_accel_scale;

        /* Gyroscope channels: 3..5 */
        scaled_out[t][3] = ((float32_t)raw_in[t][3]) * inv_gyro_scale;
        scaled_out[t][4] = ((float32_t)raw_in[t][4]) * inv_gyro_scale;
        scaled_out[t][5] = ((float32_t)raw_in[t][5]) * inv_gyro_scale;
    }
}
