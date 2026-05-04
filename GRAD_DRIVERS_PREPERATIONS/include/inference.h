#pragma once

/**
 * @file    inference.h
 * @brief   CubeAI inference wrapper for the IMU road-surface classifier.
 *
 * Exposes a 2-function API around the X-CUBE-AI generated network:
 *
 *     Inference_Init();                              // once at boot
 *     Inference_Run(ts_i8, stat_i8, &result);        // per window
 *
 * Network topology (fixed by the .tflite model):
 *   - Input 0 : stat features         50 int8   (N_STAT_FEATURES bytes)
 *   - Input 1 : time-series window   300 int8   (WINDOW_SIZE * N_FEATURES,
 *                                                row-major: t * 6 + ch)
 *   - Output 0: softmax over classes   2 int8   ([smooth, rough])
 *
 * The activations arena lives in static BSS, 8-byte aligned, sized by
 * AI_NETWORK_DATA_ACTIVATIONS_SIZE from the CubeAI generated headers.
 *
 * MISRA-C:2012 compliant. Bare-metal. No malloc, no recursion.
 */

#include "STD_TYPES.h"
#include "norm_params.h"

/*----------------------------------------------------------------------------*/
/*  Public types                                                              */
/*----------------------------------------------------------------------------*/

/** Class label encoding — must match the training pipeline. */
#define INFERENCE_LABEL_SMOOTH  0U
#define INFERENCE_LABEL_ROUGH   1U

typedef struct
{
    uint8_t  label;       /* 0 = smooth, 1 = rough                      */
    uint8_t  confidence;  /* 0..100, derived from int8 softmax margin   */
} Inference_Result_t;

/*----------------------------------------------------------------------------*/
/*  Public API                                                                */
/*----------------------------------------------------------------------------*/

/**
 * @brief  Initialise the CubeAI inference network.
 *
 * Binds the static activations arena and initialises the generated
 * network. Must be called exactly once at boot, before any
 * Inference_Run() call. Calling twice is harmless but wasteful.
 *
 * @retval 0   success
 * @retval !0  CubeAI ai_error.code (cast to uint32_t)
 */
uint32_t Inference_Init(void);

/**
 * @brief  Run inference on a quantized input pair.
 *
 * Copies the two quantized buffers into the network's input tensors,
 * runs the forward pass, then writes the argmax + confidence to *result.
 *
 * On any internal failure (uninitialised, NULL output, batch != 1) the
 * result is set to { label=0, confidence=0 } and the function returns.
 *
 * @param[in]   ts_i8    Quantized time-series, WINDOW_SIZE * N_FEATURES bytes.
 * @param[in]   stat_i8  Quantized stat features, N_STAT_FEATURES bytes.
 * @param[out]  result   Classification output. Must not be NULL.
 *
 * @note  Not re-entrant. Single-task inference assumed (uses the same
 *        static activations arena that ai_network_run reads/writes).
 * @note  Inference_Init() must have been called successfully.
 */
void Inference_Run(const int8_t  ts_i8  [WINDOW_SIZE * N_FEATURES],
                   const int8_t  stat_i8[N_STAT_FEATURES],
                   Inference_Result_t *result);
