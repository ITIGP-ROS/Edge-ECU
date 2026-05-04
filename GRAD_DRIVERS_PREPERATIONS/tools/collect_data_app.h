#ifndef COLLECT_DATA_APP_H
#define COLLECT_DATA_APP_H

/*****************************************************************************
 * collect_data_app.h — Data Collection Application Layer
 *                       STM32F401CC — Road Surface Classification Project
 *
 * PURPOSE:
 *   Public interface for the bare-metal data collection application.
 *   Encapsulates all hardware init, ISR callbacks, and the 100 Hz
 *   sample → frame → UART main loop.
 *
 * CALLER (main.c):
 *   #include "collect_data_app.h"
 *   int main(void) { CollectApp_Init(); CollectApp_Run(); }
 *
 * ARCHITECTURE:
 *   TIM2 ISR (100 Hz) → sets flag_sample_due
 *   Main loop         → MPU6050_TriggerRead → DMA callback
 *   DMA callback      → RingBuffer_Push + flag_dma_done
 *   Main loop         → RingBuffer_Pop → FRAME_BuildAndSend → UART
 *
 * CONSTRAINTS:
 *   - CollectApp_Init() must be called exactly once before CollectApp_Run()
 *   - CollectApp_Run() never returns
 *   - No FreeRTOS — bare-metal ISR + main loop pattern
 *   - No HAL, no CMSIS
 *****************************************************************************/

#include "STD_TYPES.h"

/* ================================================================
 *  Error codes
 * ================================================================ */
typedef enum
{
    COLLECT_APP_OK            = 0x00U,  /* Init succeeded                  */
    COLLECT_APP_ERROR_CLK     = 0x01U,  /* RCC init failed                 */
    COLLECT_APP_ERROR_SYSTICK = 0x02U,  /* SysTick init failed             */
    COLLECT_APP_ERROR_GPIO    = 0x03U,  /* GPIO config failed              */
    COLLECT_APP_ERROR_I2C     = 0x04U,  /* I2C or I2C_SVC init failed      */
    COLLECT_APP_ERROR_UART    = 0x05U,  /* UART or UART_SVC init failed    */
    COLLECT_APP_ERROR_MPU     = 0x06U,  /* MPU6050_Init failed             */
    COLLECT_APP_ERROR_TIM     = 0x07U   /* TIM2 init or start failed       */
} CollectApp_Error_t;

/* ================================================================
 *  Diagnostic snapshot — readable in debugger Watch panel
 * ================================================================ */
typedef struct
{
    uint8_t  app_stage;       /* Current init stage — 0xEE = fatal error  */
    uint32_t missed_ticks;    /* TIM2 ticks where previous sample not done */
    uint32_t dma_timeouts;    /* DMA transfers that did not complete in 5ms */
    uint32_t frames_sent;     /* Total frames successfully sent via UART   */
    uint32_t frames_dropped;  /* Frames dropped due to UART buffer full    */
} CollectApp_Diag_t;

/* ================================================================
 *  Public API
 * ================================================================ */

/**
 * @brief  Initialise all hardware for data collection.
 *
 *         Init sequence:
 *           1. RCC — 84 MHz HSI PLL
 *           2. SysTick — 1 ms tick
 *           3. Peripheral clocks
 *           4. I2C bus recovery (9 SCL clocks to release stuck SDA)
 *           5. GPIO — PB6/PB7 I2C, PA9/PA10 UART
 *           6. I2C1 + DMA1 Stream0 (I2C1 RX)
 *           7. USART1 IRQ TX
 *           8. MPU6050 — reset, wake, WHO_AM_I verify, config
 *           9. Ring buffer + Frame module
 *          10. TIM2 — 100 Hz hardware timer
 *          11. NVIC — arm TIM2, I2C1_EV, I2C1_ER, DMA1_S0, USART1
 *          12. TIM2 start
 *
 *         On any failure, execution halts in app_fatal_error().
 *         Inspect dbg_app_stage in debugger to identify the failure point.
 *
 * @return COLLECT_APP_OK on success (fatal error halts before return
 *         on any failure — this return is defensive only)
 */
CollectApp_Error_t CollectApp_Init(void);

/**
 * @brief  Enter the 100 Hz data collection loop. Never returns.
 *
 *         Each iteration:
 *           1. Wait for TIM2 flag (flag_sample_due)
 *           2. Trigger MPU6050 DMA read (14 bytes)
 *           3. Wait for DMA done (5 ms timeout guard)
 *           4. Pop sample from ring buffer
 *           5. Build + send 20-byte UART frame
 *
 * @note   Call only after CollectApp_Init() returns COLLECT_APP_OK.
 */
void CollectApp_Run(void);

/**
 * @brief  Return a snapshot of runtime diagnostics.
 *         Safe to call at any time — reads volatile counters atomically
 *         (32-bit aligned reads are atomic on Cortex-M4).
 *
 * @param  diag  Pointer to caller-allocated CollectApp_Diag_t struct
 *               (must not be NULL)
 */
void CollectApp_GetDiag(CollectApp_Diag_t *diag);

#endif /* COLLECT_DATA_APP_H */
