/*****************************************************************************
 * DWT_INTERFACE.h — DWT cycle-counter public API
 *                   STM32F401CC bare-metal
 *
 * Sub-microsecond timing source built on the Cortex-M4 Data Watchpoint
 * and Trace cycle counter (CYCCNT). Used for:
 *   - Inference latency measurement
 *   - Preprocessing-stage profiling
 *   - Short cycle-accurate busy-wait delays during init
 *
 * Resolution    : 1 CPU cycle  (≈ 11.9 ns @ 84 MHz)
 * Wrap interval : 2^32 / 84 MHz ≈ 51.1 seconds
 *
 * Cost model:
 *   - DWT_GetCycles():     ~3 cycles  (single load)
 *   - DWT_ElapsedSince():  ~5 cycles  (load + sub)
 *   - DWT_DelayUs(N):      N µs ± ~1 cycle (busy-wait)
 *
 * Thread/ISR safety:
 *   All read functions are ISR-safe. DWT_Init() is not — call once
 *   from main() before any other DWT_* function and before any task
 *   that might call them.
 *
 *****************************************************************************/

#ifndef DWT_INTERFACE_H
#define DWT_INTERFACE_H

#include "STD_TYPES.h"

/* ================================================================
 *  Status codes
 * ================================================================ */

typedef enum
{
    DWT_OK              = 0,  /* Counter running                            */
    DWT_ERR_NOT_PRESENT = 1   /* DWT cycle counter unavailable / locked     */
} DWT_Status_t;

/* ================================================================
 *  Public API
 * ================================================================ */

/**
 * @brief  Initialise the DWT cycle counter.
 *
 *         Enables DEMCR.TRCENA (master DWT/trace enable), resets
 *         CYCCNT, then enables CTRL.CYCCNTENA. Verifies the counter
 *         actually increments before returning success — on a few
 *         locked-down parts the cycle counter is not implemented.
 *
 *         Idempotent: safe to call multiple times. The counter is
 *         reset to 0 each time.
 *
 * @retval DWT_OK               Counter running and verified.
 * @retval DWT_ERR_NOT_PRESENT  Counter did not increment (rare).
 *
 * @note   Not ISR-safe. Call once from main() during init.
 */
DWT_Status_t DWT_Init(void);

/**
 * @brief  Read the current cycle counter value.
 *
 *         Free-running 32-bit counter at CPU clock rate (84 MHz on
 *         STM32F401CC at default project clock config). Wraps every
 *         ~51.1 seconds. Use DWT_ElapsedSince() for interval math
 *         to handle wrap correctly.
 *
 * @return Current cycle count.
 *
 * @note   ISR-safe. Single 32-bit load — atomic on Cortex-M4.
 */
uint32_t DWT_GetCycles(void);

/**
 * @brief  Compute elapsed cycles since a previously-captured start
 *         value, handling 32-bit wraparound.
 *
 *         Implementation relies on unsigned modular subtraction:
 *         (now - start) is correct as long as the elapsed interval
 *         is less than 2^32 cycles (~51 s). Caller is responsible
 *         for not measuring intervals longer than that.
 *
 * Example:
 * @code
 *     uint32_t t0 = DWT_GetCycles();
 *     do_work();
 *     uint32_t elapsed = DWT_ElapsedSince(t0);
 * @endcode
 *
 * @param  start_cycles  Cycle count captured at interval start.
 * @return Elapsed cycles (now - start_cycles), modulo 2^32.
 *
 * @note   ISR-safe.
 */
uint32_t DWT_ElapsedSince(uint32_t start_cycles);

/**
 * @brief  Convert a cycle count to microseconds at 84 MHz.
 *
 *         Truncates toward zero (integer division). For sub-µs
 *         precision use DWT_CyclesToNs() and divide externally.
 *
 *         Saturates at UINT32_MAX µs ≈ 71 minutes — well beyond
 *         any sensible single-interval measurement.
 *
 * @param  cycles  Cycle count from DWT_ElapsedSince() or DWT_GetCycles().
 * @return Microseconds (truncated).
 *
 * @note   ISR-safe.
 */
uint32_t DWT_CyclesToUs(uint32_t cycles);

/**
 * @brief  Convert a cycle count to nanoseconds at 84 MHz.
 *
 *         Uses integer math: ns = cycles * 1000 / 84.
 *         Reduced as ns = cycles * 250 / 21 to avoid 64-bit arithmetic
 *         while keeping the conversion exact.
 *
 *         Maximum input before overflow:
 *           cycles * 250 must fit in uint32_t
 *           => cycles_max = 0xFFFFFFFF / 250 = 17,179,869
 *           => ~204 ms at 84 MHz before this function overflows.
 *
 *         For longer intervals, use DWT_CyclesToUs() instead.
 *
 * @param  cycles  Cycle count, must be ≤ 17,179,869 (~204 ms).
 * @return Nanoseconds.
 *
 * @note   ISR-safe.
 */
uint32_t DWT_CyclesToNs(uint32_t cycles);

/**
 * @brief  Busy-wait blocking delay in microseconds.
 *
 *         Spins on CYCCNT for the requested duration. Cycle-accurate
 *         to within ~1 cycle but burns 100% CPU — use only for short
 *         delays during initialisation (I2C bus recovery, peripheral
 *         reset windows, MPU6050 wake-up, etc.).
 *
 *         Maximum delay: UINT32_MAX / 84 ≈ 51,124,095 µs (~51 s).
 *         Longer delays will silently saturate due to the cycle count
 *         overflowing 32 bits — DO NOT use this for delays > 1 second;
 *         use a SysTick-based delay instead.
 *
 *         Zero delay is valid and returns immediately.
 *
 * @param  us  Microseconds to wait. 0..51,000,000.
 *
 * @note   Not ISR-safe in the sense that it will block the ISR for
 *         the requested duration. Don't use in IRQ context.
 */
void DWT_DelayUs(uint32_t us);

/* USAGE
 * =====
 *
 * Measurement:
 *   uint32_t t0 = DWT_GetCycles();
 *   Inference_Run(ts_q, stat_q, &result);
 *   uint32_t cycles = DWT_ElapsedSince(t0);
 *   uint32_t us = DWT_CyclesToUs(cycles);
 *   // log "inference: %u us"
 *
 * Short delay:
 *   I2C_GenerateStop(I2C1);
 *   DWT_DelayUs(50);
 *   // bus released, safe to reconfigure
 */

#endif /* DWT_INTERFACE_H */
