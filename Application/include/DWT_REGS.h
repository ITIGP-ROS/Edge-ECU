/*****************************************************************************
 * DWT_REGS.h — Cortex-M4 DWT cycle-counter register map
 *              STM32F401CC bare-metal
 *
 * Minimal direct-pointer access to the three registers we need to
 * drive the DWT cycle counter. Intentionally avoids CMSIS Core
 * (DWT_Type, CoreDebug_Type) so this driver has zero dependency on
 * vendor headers — register addresses are fixed by the ARMv7-M
 * architecture and will not move.
 *
 * References:
 *   ARMv7-M Architecture Reference Manual (DDI0403E)
 *     §C1.6   Debug register summary       — DEMCR @ 0xE000EDFC
 *     §C1.10  Data Watchpoint and Trace    — DWT base @ 0xE0001000
 *
 *   STM32F401xC/xE Reference Manual (RM0368)
 *     §32     Cortex-M4 debug — confirms standard ARM debug map.
 *
 * Why TRCENA must be set:
 *   The DWT block on Cortex-M4 is part of the trace/debug subsystem.
 *   Although we are not actually tracing, the architecture mandates
 *   that DEMCR.TRCENA be set before any DWT register can be written
 *   or will increment. Without it, writes silently fail and CYCCNT
 *   stays at zero — a classic "DWT not ticking" pitfall.
 *
 *****************************************************************************/

#ifndef DWT_REGS_H
#define DWT_REGS_H

#include "STD_TYPES.h"

/* ================================================================
 *  Register addresses — fixed by ARMv7-M architecture
 * ================================================================ */

/**
 * @brief Debug Exception and Monitor Control Register.
 *        Bit 24 (TRCENA) is the master enable for DWT/ITM/ETM.
 *        Address: 0xE000EDFC (Private Peripheral Bus)
 */
#define DWT_DEMCR     (*(volatile uint32_t *)0xE000EDFCU)

/**
 * @brief DWT Control Register.
 *        Bit 0 (CYCCNTENA) enables the cycle counter.
 *        Address: 0xE0001000 (Private Peripheral Bus)
 */
#define DWT_CTRL      (*(volatile uint32_t *)0xE0001000U)

/**
 * @brief DWT Cycle Count Register.
 *        Free-running 32-bit counter incremented every CPU cycle
 *        when CYCCNTENA == 1. Wraps at 2^32. Software-writable
 *        (write 0 to reset).
 *        Address: 0xE0001004 (Private Peripheral Bus)
 */
#define DWT_CYCCNT    (*(volatile uint32_t *)0xE0001004U)

/* ================================================================
 *  Bit masks
 * ================================================================ */

/** DEMCR.TRCENA — enables DWT/ITM/ETM trace blocks (DEMCR bit 24). */
#define DWT_DEMCR_TRCENA      (1UL << 24U)

/** DWT_CTRL.CYCCNTENA — enables the cycle counter (CTRL bit 0). */
#define DWT_CTRL_CYCCNTENA    (1UL << 0U)

#endif /* DWT_REGS_H */
