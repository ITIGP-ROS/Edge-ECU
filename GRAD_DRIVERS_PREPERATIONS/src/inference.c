/**
 * @file    inference.c
 * @brief   CubeAI inference wrapper — see inference.h.
 *
 * MISRA-C:2012 compliant, bare-metal, no malloc.
 */

#include <string.h>             /* memcpy */

#include "STD_TYPES.h"
#include "norm_params.h"
#include "inference.h"


/* CubeAI generated headers — provide ai_handle, ai_buffer, ai_error,
   AI_NETWORK_DATA_ACTIVATIONS_SIZE, ai_network_create_and_init(),
   ai_network_inputs_get(), ai_network_outputs_get(), ai_network_run(). */
#include "network.h"
#include "network_data.h"

/*----------------------------------------------------------------------------*/
/*  Compile-time sanity: the model's tensor sizes must match our constants.  */
/*  AI_NETWORK_IN_1_SIZE / IN_2_SIZE / OUT_1_SIZE are emitted by CubeAI in   */
/*  network.h. If your generator names them differently (e.g.,               */
/*  AI_NETWORK_IN_1_SIZE_BYTES), adjust below.                               */
/*----------------------------------------------------------------------------*/
#if defined(AI_NETWORK_IN_1_SIZE) && defined(AI_NETWORK_IN_2_SIZE)
#  if (AI_NETWORK_IN_1_SIZE != N_STAT_FEATURES)
#    error "CubeAI input[0] size mismatch — expected N_STAT_FEATURES (50)."
#  endif
#  if (AI_NETWORK_IN_2_SIZE != (WINDOW_SIZE * N_FEATURES))
#    error "CubeAI input[1] size mismatch — expected WINDOW_SIZE*N_FEATURES."
#  endif
#endif

/*----------------------------------------------------------------------------*/
/*  Module state                                                              */
/*----------------------------------------------------------------------------*/

/** Activations arena: static BSS, 8-byte aligned per CubeAI ABI. */
static ai_u8 s_activations[AI_NETWORK_DATA_ACTIVATIONS_SIZE]
    __attribute__((aligned(8)));

static ai_handle s_network     = AI_HANDLE_NULL;
static uint8_t   s_initialised = 0U;

/*----------------------------------------------------------------------------*/
/*  Confidence scaling                                                        */
/*  margin = winner - loser  in [0..255] in int8 softmax space.               */
/*  We map linearly to [0..100]. Pre-multiply by 100 to keep precision.       */
/*----------------------------------------------------------------------------*/
#define INFERENCE_MARGIN_MAX   255

/*----------------------------------------------------------------------------*/
/*  Public API                                                                */
/*----------------------------------------------------------------------------*/

uint32_t Inference_Init(void)
{
    /* CubeAI activations binding: array of buffer base addresses, one
       entry per declared activations buffer. Most generated networks
       expose exactly one. */
    const ai_handle act_addr[] = { s_activations };

    ai_error err = ai_network_create_and_init(&s_network, act_addr, NULL);
    if (err.type != AI_ERROR_NONE)
    {
        s_initialised = 0U;
        return (uint32_t)err.code;
    }

    s_initialised = 1U;
    return 0U;
}

void Inference_Run(const int8_t  ts_i8  [WINDOW_SIZE * N_FEATURES],
                   const int8_t  stat_i8[N_STAT_FEATURES],
                   Inference_Result_t *result)
{
    /* Defensive: caller may not have checked Inference_Init() return. */
    if ((s_initialised == 0U) || (result == NULL))
    {
        if (result != NULL)
        {
            result->label      = INFERENCE_LABEL_SMOOTH;
            result->confidence = 0U;
        }
        return;
    }

    /* Pre-fill with a defined "fail" value so a mid-function abort
       still leaves *result in a known state. */
    result->label      = INFERENCE_LABEL_SMOOTH;
    result->confidence = 0U;

    /* Acquire CubeAI-managed I/O descriptors. */
    ai_buffer *inputs  = ai_network_inputs_get(s_network, NULL);
    ai_buffer *outputs = ai_network_outputs_get(s_network, NULL);

    if ((inputs == NULL) || (outputs == NULL) ||
        (inputs[0].data == NULL) || (inputs[1].data == NULL) ||
        (outputs[0].data == NULL))
    {
        return;
    }

    /* Copy quantized inputs. Order is fixed by the .tflite graph:
         inputs[0] = stat features  (N_STAT_FEATURES bytes)
         inputs[1] = time-series    (WINDOW_SIZE * N_FEATURES bytes,
                                     row-major t * 6 + ch). */
    (void)memcpy(inputs[0].data, stat_i8,
                 (size_t)N_STAT_FEATURES);
    (void)memcpy(inputs[1].data, ts_i8,
                 (size_t)(WINDOW_SIZE * N_FEATURES));

    /* Forward pass. Returns the number of batches processed; we expect 1. */
    ai_i32 batch = ai_network_run(s_network, inputs, outputs);
    if (batch != (ai_i32)1)
    {
        return;
    }

    /* Output layout: 2 int8 values, softmax over [smooth, rough].
       Argmax in int8 space is equivalent to argmax in float space
       (a monotonic affine dequantization). */
    const int8_t *out = (const int8_t *)outputs[0].data;

    uint8_t label;
    int16_t winner;
    int16_t loser;

    if (out[1] > out[0])
    {
        label  = INFERENCE_LABEL_ROUGH;
        winner = (int16_t)out[1];
        loser  = (int16_t)out[0];
    }
    else
    {
        label  = INFERENCE_LABEL_SMOOTH;
        winner = (int16_t)out[0];
        loser  = (int16_t)out[1];
    }

    /* Confidence in int8 margin space.
       Range: 0..255 (winner range -128..+127, loser range -128..+127,
       difference clamped to non-negative by the argmax above).
       Pre-multiply by 100 to preserve resolution before integer divide. */
    int32_t margin = (int32_t)winner - (int32_t)loser;
    if (margin < 0)
    {
        margin = 0;                        /* defensive — argmax guarantees >=0 */
    }
    if (margin > INFERENCE_MARGIN_MAX)
    {
        margin = INFERENCE_MARGIN_MAX;     /* defensive — physically impossible */
    }

    result->label      = label;
    result->confidence = (uint8_t)((margin * (int32_t)100) /
                                   (int32_t)INFERENCE_MARGIN_MAX);
}
