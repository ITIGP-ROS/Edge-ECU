/**
 * @file    verify.c
 * @brief   Host-side verification harness for the FFT-independent stages of
 *          the embedded preprocessing pipeline (scale + ts-quantize).
 *
 * Features_Extract / Quantize_Stat are NOT verified here because they
 * depend on CMSIS-DSP (arm_rfft_fast_f32). Those are validated on-target.
 *
 * Run on the dev PC before flashing the MCU.
 *
 * Build:
 *   make
 *
 * Exit code = number of failing assertions (0 = all pass).
 *
 * --------------------------------------------------------------------------
 * Test-vector naming convention (must match tools/gen_test_vectors.py):
 *   test_raw           [N_TEST_VECTORS][WINDOW_SIZE][N_FEATURES]
 *   expected_scaled    [N_TEST_VECTORS][WINDOW_SIZE][N_FEATURES]
 *   expected_ts_int8   [N_TEST_VECTORS][WINDOW_SIZE * N_FEATURES]
 *
 * If the generator uses different names (e.g. tv_*), update either the
 * generator or this file — they must agree.
 * --------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>

#include "norm_params.h"
#include "scale.h"
#include "quantize.h"

#include "test_vectors.h"

/*----------------------------------------------------------------------------*/
/*  ANSI colours (optional — strip if your terminal doesn't like them)        */
/*----------------------------------------------------------------------------*/
#define C_RED    "\x1b[31m"
#define C_GREEN  "\x1b[32m"
#define C_YELLOW "\x1b[33m"
#define C_RESET  "\x1b[0m"

/*----------------------------------------------------------------------------*/
/*  Pretty-printing helpers                                                   */
/*----------------------------------------------------------------------------*/
static void print_pass(const char *what, int idx)
{
    printf("  " C_GREEN "PASS" C_RESET "  vec[%d]  %s\n", idx, what);
}

static void print_fail(const char *what, int idx)
{
    printf("  " C_RED   "FAIL" C_RESET "  vec[%d]  %s\n", idx, what);
}

/*----------------------------------------------------------------------------*/
/*  1) verify_scale                                                           */
/*----------------------------------------------------------------------------*/
static int verify_scale(int test_idx, float epsilon)
{
    float32_t scaled[WINDOW_SIZE][N_FEATURES];

    Scale_RawWindow(test_raw[test_idx], scaled);

    int       ok            = 1;
    float     max_abs_err   = 0.0f;
    int       worst_t       = -1;
    int       worst_ch      = -1;

    for (int t = 0; t < WINDOW_SIZE; ++t)
    {
        for (int ch = 0; ch < N_FEATURES; ++ch)
        {
            const float c_val  = scaled[t][ch];
            const float py_val = expected_scaled[test_idx][t][ch];
            const float err    = fabsf(c_val - py_val);

            if (err > max_abs_err)
            {
                max_abs_err = err;
                worst_t     = t;
                worst_ch    = ch;
            }
            if (err > epsilon)
            {
                ok = 0;
            }
        }
    }

    if (ok)
    {
        print_pass("Scale_RawWindow", test_idx);
        printf("        max_abs_err = %.3e (eps = %.1e)\n",
               max_abs_err, epsilon);
    }
    else
    {
        print_fail("Scale_RawWindow", test_idx);
        printf("        worst at t=%d ch=%d : C=%.8f  PY=%.8f  err=%.3e\n",
               worst_t, worst_ch,
               scaled[worst_t][worst_ch],
               expected_scaled[test_idx][worst_t][worst_ch],
               max_abs_err);
    }
    return ok;
}

/*----------------------------------------------------------------------------*/
/*  2) verify_quantize_ts (exact match)                                       */
/*----------------------------------------------------------------------------*/
static int verify_quantize_ts(int test_idx)
{
    float32_t scaled[WINDOW_SIZE][N_FEATURES];
    int8_t    q_ts [WINDOW_SIZE * N_FEATURES];

    Scale_RawWindow(test_raw[test_idx], scaled);
    Quantize_TS(scaled, q_ts);

    int ok      = 1;
    int n_diff  = 0;
    int max_lsb = 0;

    for (int i = 0; i < WINDOW_SIZE * N_FEATURES; ++i)
    {
        const int c_val  = q_ts[i];
        const int py_val = expected_ts_int8[test_idx][i];
        const int diff   = abs(c_val - py_val);

        if (diff > max_lsb) max_lsb = diff;
        if (diff != 0)
        {
            ++n_diff;
            if (n_diff <= 5)   /* show first few */
            {
                if (ok) { print_fail("Quantize_TS", test_idx); }
                printf("        [%3d] (t=%2d ch=%d)  C=%4d  PY=%4d  diff=%d\n",
                       i, i / N_FEATURES, i % N_FEATURES,
                       c_val, py_val, c_val - py_val);
            }
            ok = 0;
        }
    }

    if (ok)
    {
        print_pass("Quantize_TS (exact)", test_idx);
    }
    else
    {
        printf("        %d / %d samples differ (max %d LSB)\n",
               n_diff, WINDOW_SIZE * N_FEATURES, max_lsb);
    }
    return ok;
}

/*----------------------------------------------------------------------------*/
/*  main                                                                      */
/*----------------------------------------------------------------------------*/
int main(void)
{
    printf("=========================================================\n");
    printf("  Host verification: Scale + Quantize_TS only\n");
    printf("  (FFT-dependent steps verified on MCU)\n");
    printf("  %d test vectors, channels=%d, window=%d\n",
           N_TEST_VECTORS, N_FEATURES, WINDOW_SIZE);
    printf("=========================================================\n");

    int total_fails = 0;
    for (int i = 0; i < N_TEST_VECTORS; ++i)
    {
        printf("\n--- Test vector %d ------------------------------------\n", i);
        total_fails += !verify_scale       (i, 1.0e-6f);
        total_fails += !verify_quantize_ts (i);
    }

    printf("\n=========================================================\n");
    if (total_fails == 0)
    {
        printf("  " C_GREEN "ALL TESTS PASSED" C_RESET "  (%d checks)\n",
               N_TEST_VECTORS * 2);
    }
    else
    {
        printf("  " C_RED "FAILURES: %d" C_RESET " of %d checks\n",
               total_fails, N_TEST_VECTORS * 2);
    }
    printf("=========================================================\n");
    return total_fails;
}
