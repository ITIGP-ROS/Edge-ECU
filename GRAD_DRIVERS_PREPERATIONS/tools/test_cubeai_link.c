/**
 * @file  test_cubeai_link.c
 * @brief Linker-keep stubs for CubeAI + preprocessing + inference modules,
 *        plus a one-shot frame-builder verification helper.
 *
 * Without explicit references from main(), --gc-sections strips these.
 * This file forces them into the link image. Not called from production
 * code paths.
 */

#include "STD_TYPES.h"
#include "norm_params.h"
#include "scale.h"
#include "features.h"
#include "quantize.h"
#include "inference.h"
#include "vote.h"
#include "FRAME.h"      /* <-- ADDED */

/* ================================================================
 *  CubeAI keep-alive dummies (existing)
 * ================================================================ */
static int16_t   s_dummy_raw  [WINDOW_SIZE][N_FEATURES];
static float32_t s_dummy_scl  [WINDOW_SIZE][N_FEATURES];
static float32_t s_dummy_feat [N_STAT_FEATURES];
static int8_t    s_dummy_ts   [WINDOW_SIZE * N_FEATURES];
static int8_t    s_dummy_stat [N_STAT_FEATURES];
static Inference_Result_t s_dummy_res;

/* ================================================================
 *  Frame-builder verification buffer  (ADDED)
 *
 *  Inspect s_frame_buf[0..s_frame_len-1] in your debugger after
 *  calling test_frame_build(). Should be exactly 13 bytes:
 *
 *    AA 01 06 01 5C 12 34 56 78 [CRC32_BE: 4 bytes]
 *
 *  Compare those 4 CRC bytes against the Python reference output.
 * ================================================================ */
static uint8_t  s_frame_buf[16];
static uint16_t s_frame_len;

void test_frame_build(void);
void test_frame_build(void)
{
    /* Classification frame:
     *   payload[0] = label   = 0x01 (e.g. "rough")
     *   payload[1] = conf    = 0x5C (= 92)
     *   payload[2..5] = ts   = 0x12345678 big-endian
     */
    static const uint8_t payload[6] = {
        0x01U, 0x5CU, 0x12U, 0x34U, 0x56U, 0x78U
    };

    s_frame_len = 0U;
    (void)Frame_Build(FRAME_TYPE_CLASSIFICATION,
                      payload, (uint8_t)sizeof payload,
                      s_frame_buf, (uint16_t)sizeof s_frame_buf,
                      &s_frame_len);

    /* Set a debugger breakpoint on the line below.
     * Expected state when hit:
     *   s_frame_len == 13
     *   s_frame_buf == { 0xAA, 0x01, 0x06, 0x01, 0x5C, 0x12, 0x34, 0x56, 0x78,
     *                    CRC_HI, CRC_2, CRC_1, CRC_LO }
     */
    __asm volatile ("nop");   /* breakpoint anchor */
}

/* ================================================================
 *  Single keepalive entry point — call once from main() to prevent gc
 * ================================================================ */
void test_cubeai_init(void);
void test_cubeai_init(void)
{
    Features_Init();
    (void)Inference_Init();
    Scale_RawWindow(s_dummy_raw, s_dummy_scl);
    Features_Extract(s_dummy_scl, s_dummy_feat);
    Quantize_NormalizeStat(s_dummy_feat);
    Quantize_TS(s_dummy_scl, s_dummy_ts);
    Quantize_Stat(s_dummy_feat, s_dummy_stat);
    Inference_Run(s_dummy_ts, s_dummy_stat, &s_dummy_res);

    /* Frame format verification — runs once at boot.
     * Inspect s_frame_buf in the debugger Watch panel. */
    test_frame_build();
}

const uint8_t *test_frame_get_buf(void);
uint16_t       test_frame_get_len(void);

const uint8_t *test_frame_get_buf(void) { return s_frame_buf; }
uint16_t       test_frame_get_len(void) { return s_frame_len; }