#pragma once

/**
 * @file    mcu_verify.h
 * @brief   One-shot, boot-time MCU-side preprocessing verification.
 *
 * Runs the full preprocessing pipeline:
 *     Scale -> Features -> NormalizeStat -> QuantizeStat / QuantizeTS
 * on a single hardcoded test vector embedded in the .rodata section,
 * then dumps the intermediate and final outputs over UART1 in a
 * Python-parseable ASCII format for offline byte-for-byte comparison
 * against the training pipeline reference outputs.
 *
 * Output format on UART1 (921600 baud, 8N1):
 *
 *   ===VERIFY_START===
 *   FEATURES_0,<f0>,<f1>,...,<f24>
 *   FEATURES_1,<f25>,<f26>,...,<f49>
 *   STAT_INT8,<s0>,<s1>,...,<s49>
 *   TS_INT8_0,<t0>,<t1>,...,<t99>
 *   TS_INT8_1,<t100>,<t101>,...,<t199>
 *   TS_INT8_2,<t200>,<t201>,...,<t299>
 *   INFERENCE,<label>,<confidence>
 *   ===VERIFY_END===
 *
 * Float values printed with 8 decimal digits (manual deterministic
 * formatter — does NOT rely on newlib `%f` support).
 * Int8 values printed as signed decimal integers.
 * INFERENCE: <label> is 0=smooth, 1=rough; <confidence> is 0..100
 *            derived from the int8 softmax margin.
 *
 * Preconditions (must be true before calling):
 *   - System clock initialised (RCC).
 *   - GPIO for UART1 TX configured.
 *   - UART_SVC_Init(UART1_ID, ..., UART_SVC_TX_MODE_DMA, ...) called.
 *   - CubeAI generated network code linked into the image
 *     (network.c / network_data.c).
 *
 * The function calls Features_Init() and Inference_Init() internally —
 * caller does not need to.
 *
 * @note  Blocks until UART transmission completes (~70 ms @ 921600 baud).
 * @note  One-shot. NOT re-entrant. Uses ~2.5 KB of static BSS for working
 *        buffers. Uses module-static FFT state inside features.c, so
 *        must not run concurrently with any other Features_Extract caller.
 * @note  Intended to be called ONCE at the top of main(), before any
 *        application logic, scheduler, or interrupt-driven IMU sampling.
 */

void McuVerify_RunOnce(void);
