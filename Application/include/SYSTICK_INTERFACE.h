#ifndef SYSTICK_INTERFACE_H
#define SYSTICK_INTERFACE_H

#include "STD_TYPES.h"

/* ============================================================
 * Error Status Enum
 * Explicit hex values + U suffix per MISRA Rule 8.12
 * ============================================================ */
typedef enum {
    SYSTICK_OK                  = 0x00U,
    SYSTICK_ERR_INVALID_RELOAD  = 0x01U,
    SYSTICK_ERR_INVALID_CLK     = 0x02U,
    SYSTICK_ERR_RCC_FAIL        = 0x03U,
    SYSTICK_ERR_NULL_POINTER    = 0x04U,
    SYSTICK_ERR_NOT_INITIALIZED = 0x05U,
    SYSTICK_ERR_UNKNOWN_CLK     = 0x06U
} Systick_Error_t;

/* ============================================================
 * Clock Source Enum
 * - Renamed type:        clk_src_t       → Systick_ClkSrc_t
 * - Renamed enumerators: UPPER_CASE with SYSTICK_ prefix
 * - Explicit values + U suffix per MISRA
 *
 * NOTE on bit meaning (ARMv7-M PM0214 §4.5):
 *   CLKSOURCE = 1  →  processor clock (AHB)
 *   CLKSOURCE = 0  →  external reference clock (AHB / 8)
 * ============================================================ */
typedef enum {
    SYSTICK_CLK_PROCESSOR = 0U,   /* AHB directly   — CLKSOURCE = 1 */
    SYSTICK_CLK_AHB_DIV8  = 1U    /* AHB / 8        — CLKSOURCE = 0 */
} Systick_ClkSrc_t;

/* ============================================================
 * PUBLIC API
 * These are the functions the Application Layer can use.
 * ============================================================ */

/**
 * @brief Initialize SysTick hardware (clock source only)
 * @param clk_src   SYSTICK_CLK_PROCESSOR or SYSTICK_CLK_AHB_DIV8
 * @return error status
 */
Systick_Error_t Systick_Init(Systick_ClkSrc_t clk_src);

/**
 * @brief Start SysTick counter and interrupt
 * @note  Returns SYSTICK_ERR_INVALID_RELOAD if RELOAD is still 0
 * @return error status
 */
Systick_Error_t Systick_Start(void);

/**
 * @brief Stop SysTick counter and interrupt
 */
void Systick_Stop(void);

/**
 * @brief Set new reload value based on desired period in milliseconds
 * @param period_ms   Desired tick period in ms
 * @return error status
 */
Systick_Error_t Systick_SetReload(uint32_t period_ms);

/**
 * @brief Register a callback function executed inside SysTick_Handler
 * @param cbf      pointer to function (void func(void))
 * @return error status
 */
Systick_Error_t Systick_ConfigureCallback(void (*cbf)(void));

/**
 * @brief  Get current cumulative tick count (incremented each SysTick ISR)
 * @note   32-bit aligned read on Cortex-M4 is inherently atomic — no
 *         critical section needed.
 * @return tick count (wraps at UINT32_MAX)
 */
uint32_t Systick_GetTick(void);

/**
 * @brief SysTick IRQ Handler called automatically by vector table
 * This MUST be present in interface so the user knows it exists.
 */
void SysTick_Handler(void);

#endif /* SYSTICK_INTERFACE_H */
