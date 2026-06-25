/*****************************************************************************
 * DWT.c — Cortex-M4 DWT cycle-counter implementation
 *         STM32F401CC bare-metal
 *
 * The DWT cycle counter is a free-running 32-bit counter that increments
 * on every CPU clock cycle. Once enabled, it requires no servicing —
 * reading it is a single 32-bit load. This makes it the cheapest and
 * most precise timing source on the chip.
 *
 * Initialisation sequence (per ARMv7-M ARM §C1.10):
 *   1. Set DEMCR.TRCENA      — master enable for trace/DWT block
 *   2. Write 0 to DWT_CYCCNT — ensure known starting state
 *   3. Set DWT_CTRL.CYCCNTENA — enable the counter
 *   4. Read CYCCNT twice     — verify it is actually incrementing
 *
 * Step 4 catches the case where the cycle counter is not implemented
 * on the part (rare for STM32F4 but allowed by the architecture —
 * DWT_CTRL.NOCYCCNT bit indicates this; we use the cheaper read-twice
 * runtime check instead of decoding NOCYCCNT).
 *
 *****************************************************************************/

#include "DWT_INTERFACE.h"
#include "DWT_REGS.h"

/* ================================================================
 *  Local constants
 * ================================================================ */

/* CPU clock matches main.c's RCC_INIT_84MHz_HSI(). If the project
 * ever moves to 96 MHz HSE or 100 MHz overdrive, update this single
 * constant — every conversion in this file derives from it. */
#define DWT_CPU_CLOCK_HZ      84000000UL

/* Cycles per microsecond at the project clock rate. Compile-time
 * constant — the compiler folds the division. */
#define DWT_CYCLES_PER_US     (DWT_CPU_CLOCK_HZ / 1000000UL)   /* = 84 */

/* For ns conversion: 1000 / 84 = 250 / 21 (exact reduction).
 * Used to avoid 64-bit math while keeping precision. */
#define DWT_NS_NUMERATOR      250UL
#define DWT_NS_DENOMINATOR    21UL

/* ================================================================
 *  Public API
 * ================================================================ */

DWT_Status_t DWT_Init(void)
{
    uint32_t a;
    uint32_t b;

    /* 1) Enable the trace/debug master switch.
     *    Required before any DWT register write will take effect. */
    DWT_DEMCR |= DWT_DEMCR_TRCENA;

    /* 2) Reset the counter to a known state. */
    DWT_CYCCNT = 0U;

    /* 3) Enable the cycle counter. */
    DWT_CTRL |= DWT_CTRL_CYCCNTENA;

    /* 4) Verify it actually started incrementing.
     *    Two consecutive reads must differ — at 84 MHz the counter
     *    advances every ~12 ns, so by the time the second load
     *    instruction completes, CYCCNT is guaranteed to have moved.
     *
     *    If it didn't, the counter is unimplemented or locked. */
    a = DWT_CYCCNT;
    b = DWT_CYCCNT;

    if (a == b)
    {
        return DWT_ERR_NOT_PRESENT;
    }

    return DWT_OK;
}

uint32_t DWT_GetCycles(void)
{
    return DWT_CYCCNT;
}

uint32_t DWT_ElapsedSince(uint32_t start_cycles)
{
    /* Unsigned modular subtraction: correct under 32-bit wrap as long
     * as the true elapsed interval is < 2^32 cycles (~51 s @ 84 MHz).
     *
     * Example: start = 0xFFFFFFF0, now = 0x00000010
     *          (now - start) = 0x00000020 = 32 cycles. ✔ */
    return DWT_CYCCNT - start_cycles;
}

uint32_t DWT_CyclesToUs(uint32_t cycles)
{
    return cycles / DWT_CYCLES_PER_US;
}

uint32_t DWT_CyclesToNs(uint32_t cycles)
{
    /* ns = cycles * (1000 / 84) = cycles * 250 / 21 (exact).
     *
     * Overflow analysis:
     *   cycles * 250 must fit in uint32_t.
     *   Max cycles = 0xFFFFFFFFU / 250U = 17,179,869
     *              ≈ 204.5 ms of cycles at 84 MHz.
     *
     * Caller is responsible for staying under that bound — this
     * function is intended for measuring tens-of-µs to a few-ms
     * intervals where ns precision matters. */
    return (cycles * DWT_NS_NUMERATOR) / DWT_NS_DENOMINATOR;
}

void DWT_DelayUs(uint32_t us)
{
    uint32_t start;
    uint32_t target_cycles;

    /* Zero is a legal no-op. Guard avoids an unnecessary read. */
    if (us == 0U)
    {
        return;
    }

    start         = DWT_CYCCNT;
    target_cycles = us * DWT_CYCLES_PER_US;

    /* Spin until (now - start) >= target.
     * Unsigned subtraction handles wrap during the wait, as long
     * as target_cycles itself does not overflow uint32_t. The doc
     * comment in the header bounds 'us' to keep this true. */
    while ((DWT_CYCCNT - start) < target_cycles)
    {
        /* spin — no __NOP() needed; the load itself is the work */
    }
}
