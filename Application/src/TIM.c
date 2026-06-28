/*****************************************************************************
 * TIM.c — General Purpose Timer Driver Implementation (TIM2–TIM5)
 *         STM32F401CC, Reference Manual RM0368
 *
 * Hardware dependencies:
 *   - TIM2, TIM3, TIM4, TIM5 peripherals (APB1 bus)
 *   - RCC APB1ENR for clock gating
 *   - NVIC for interrupt delivery (NOT managed by this driver — T-I05)
 *
 * Primary use case: TIM2 at 100 Hz update event to trigger MPU6050
 * DMA reads for road surface data collection (bare-metal, no FreeRTOS).
 *
 *****************************************************************************/

#include "TIM_INTERFACE.h"
#include "TIM_REGS.h"
#include "RCC_INTERFACE.h"

/* ========================================================= */
/* ================= Static State ========================== */
/* ========================================================= */

/*
 * T-C13: All module state is file-scope static.
 *
 * tim_initialized[] — tracks whether TIM_Init has been called for each
 *   timer instance. Not volatile: only written/read in task context
 *   (TIM_Init, TIM_DeInit, and public API validation).
 *
 * tim_running[] — volatile because it is written in task context
 *   (TIM_Start, TIM_Stop) and could be read in ISR context or
 *   inspected by a debugger asynchronously.
 *
 * tim_callback[] / tim_ctx[] — written in task context, read in ISR.
 *   Single aligned pointer writes are atomic on Cortex-M4, so no
 *   critical section is needed. However, the caller must ensure
 *   the callback is ISR-safe (no blocking, no printf, no HAL).
 *
 * Index mapping: 0 = TIM2, 1 = TIM3, 2 = TIM4, 3 = TIM5
 */
static uint8_t          tim_initialized[4U] = {0U, 0U, 0U, 0U};
static volatile uint8_t tim_running[4U]     = {0U, 0U, 0U, 0U};
static TIM_Callback_t   tim_callback[4U]    = {NULL, NULL, NULL, NULL};
static void            *tim_ctx[4U]         = {NULL, NULL, NULL, NULL};

/* Input capture callbacks */
static TIM_Callback_t   tim_ic_callback[4U][4U] = {
    {NULL, NULL, NULL, NULL},
    {NULL, NULL, NULL, NULL},
    {NULL, NULL, NULL, NULL},
    {NULL, NULL, NULL, NULL}
};
static void            *tim_ic_ctx[4U][4U] = {
    {NULL, NULL, NULL, NULL},
    {NULL, NULL, NULL, NULL},
    {NULL, NULL, NULL, NULL},
    {NULL, NULL, NULL, NULL}
};

/* ========================================================= */
/* ================= Debug Breadcrumbs ===================== */
/* ========================================================= */

/*
 * Per-instance debug stage — readable in a Watch panel or memory view.
 * volatile because these are written in both task and ISR context.
 *
 * Stage values:
 *   0    = uninitialised
 *   1    = TIM_Init entered
 *   2    = RCC clock enabled
 *   3    = PSC/ARR/CR1 written
 *   4    = EGR.UG fired, SR.UIF cleared
 *   5    = UIE enabled in DIER
 *   99   = Init SUCCESS
 *   0xA0 = TIM2_IRQHandler entered
 *   0xA1 = TIM3_IRQHandler entered
 *   0xA2 = TIM4_IRQHandler entered
 *   0xA3 = TIM5_IRQHandler entered
 */
static volatile uint8_t dbg_tim_stage[4U] = {0U, 0U, 0U, 0U};

/* ========================================================= */
/* ================= Static Helpers ======================== */
/* ========================================================= */

/*
 * get_tim_regs — Return the TIM_REGS_t pointer for a given TIM_Id_t.
 *
 * Returns NULL for invalid id. Every public function calls this as the
 * single validation point for ID range — no duplicate range checks needed.
 */
static volatile TIM_REGS_t *get_tim_regs(TIM_Id_t id)
{
    volatile TIM_REGS_t *result;

    switch (id)
    {
        case TIM_ID_2:
            result = TIM2;
            break;
        case TIM_ID_3:
            result = TIM3;
            break;
        case TIM_ID_4:
            result = TIM4;
            break;
        case TIM_ID_5:
            result = TIM5;
            break;
        default:
            result = NULL;
            break;
    }

    return result;
}

/*
 * get_rcc_periph — Map TIM_Id_t to the RCC_Peripheral_t enum value
 *                  used by RCC_EN_CLK_PERIPHERAL / RCC_DisablePeripheralClock.
 *
 * Returns 0xFFU cast to RCC_Peripheral_t for unknown id — caller must
 * check before passing to RCC functions.
 *
 * Mapping (from RCC_INTERFACE.h):
 *   TIM_ID_2 → PERIPH_TIM2 (0x40U, APB1ENR bit 0)
 *   TIM_ID_3 → PERIPH_TIM3 (0x41U, APB1ENR bit 1)
 *   TIM_ID_4 → PERIPH_TIM4 (0x42U, APB1ENR bit 2)
 *   TIM_ID_5 → PERIPH_TIM5 (0x43U, APB1ENR bit 3)
 */
static RCC_Peripheral_t get_rcc_periph(TIM_Id_t id)
{
    RCC_Peripheral_t result;

    switch (id)
    {
        case TIM_ID_2:
            result = PERIPH_TIM2;
            break;
        case TIM_ID_3:
            result = PERIPH_TIM3;
            break;
        case TIM_ID_4:
            result = PERIPH_TIM4;
            break;
        case TIM_ID_5:
            result = PERIPH_TIM5;
            break;
        default:
            result = (RCC_Peripheral_t)0xFFU;
            break;
    }

    return result;
}

/* ========================================================= */
/* ================= Initialization & Control ============== */
/* ========================================================= */

/*
 * TIM_Init — Initialize a general purpose timer.
 *
 * See TIM_INTERFACE.h for full documentation.
 * Follows mandatory sequence T-C01..T-C12.
 *
 * Does NOT set CEN — call TIM_Start separately.
 * Does NOT enable NVIC — caller does that after TIM_Init (T-I05).
 */
TIM_Error_t TIM_Init(const TIM_Config_t *cfg)
{
    volatile TIM_REGS_t *tim;
    RCC_Peripheral_t     rcc_periph;
    RCC_Error_t          rcc_err;
    uint32_t             cr1_val;
    uint32_t             idx;

    /* -------------------------------------------------------
     * T-C01: Validate cfg pointer
     * ------------------------------------------------------- */
    if (cfg == NULL)
    {
        return TIM_ERROR_INVALID_PARAM;
    }

    /* -------------------------------------------------------
     * T-C02: Validate id range (0–3 maps to TIM2–TIM5)
     * ------------------------------------------------------- */
    tim = get_tim_regs(cfg->id);
    if (tim == NULL)
    {
        return TIM_ERROR_INVALID_TIM;
    }

    idx = (uint32_t)cfg->id;

    /* Debug breadcrumb: TIM_Init entered */
    dbg_tim_stage[idx] = 1U;

    /* -------------------------------------------------------
     * T-C03: Reject if already running (CEN=1)
     *        Check both software flag and hardware register
     *        for robustness against out-of-order calls.
     * ------------------------------------------------------- */
    if (tim_running[idx] != 0U)
    {
        return TIM_ERROR_BUSY;
    }
    if ((tim->CR1.ALL & (1U << 0U)) != 0U)
    {
        return TIM_ERROR_BUSY;
    }

    /* -------------------------------------------------------
     * T-C04: Enable RCC clock via RCC_EN_CLK_PERIPHERAL
     *        Must be done before any register access to this timer.
     * ------------------------------------------------------- */
    rcc_periph = get_rcc_periph(cfg->id);
    if ((uint8_t)rcc_periph == 0xFFU)
    {
        return TIM_ERROR_INVALID_TIM;
    }

    rcc_err = RCC_EN_CLK_PERIPHERAL(rcc_periph);
    if (rcc_err != RCC_OK)
    {
        return TIM_ERROR_INVALID_PARAM;
    }

    /* Debug breadcrumb: RCC clock enabled */
    dbg_tim_stage[idx] = 2U;

    /* -------------------------------------------------------
     * T-C05: Write CR1 with CEN=0 (stop any residual counting)
     *        Configure mode (DIR) and ARPE.
     *        Do NOT set CEN here — TIM_Start is separate.
     * ------------------------------------------------------- */
    cr1_val = 0U;

    /* DIR bit [4]: 0 = up, 1 = down */
    if (cfg->mode == TIM_MODE_DOWN)
    {
        cr1_val |= (1U << 4U);
    }

    /* ARPE bit [7]: auto-reload preload enable */
    if (cfg->arpe == TIM_ARPE_ENABLE)
    {
        cr1_val |= (1U << 7U);
    }

    tim->CR1.ALL = cr1_val;

    /* -------------------------------------------------------
     * T-C06: Write PSC register (cfg->prescaler directly, no ±1)
     *
     * T-I01: Hardware divides by (PSC+1). Pass 8399U for ÷8400.
     *        Do not subtract 1 — the value is written as-is.
     * ------------------------------------------------------- */
    tim->PSC.ALL = cfg->prescaler;

    /* -------------------------------------------------------
     * T-C07: Write ARR register (cfg->period directly, no ±1)
     *
     * T-I01: Hardware counts (ARR+1) ticks. Pass 99U for 100 ticks.
     * T-03:  ARR is 32-bit on TIM2/TIM5, 16-bit on TIM3/TIM4.
     *        Upper 16 bits are reserved/ignored on TIM3/TIM4.
     * ------------------------------------------------------- */
    tim->ARR.ALL = cfg->period;

    /* Debug breadcrumb: PSC/ARR/CR1 written */
    dbg_tim_stage[idx] = 3U;

    /* -------------------------------------------------------
     * T-C08: Fire EGR.UG = 1 to load shadow registers
     *
     * TRAP 4 (EGR write-only): EGR returns 0x0000 on read per RM0368.
     * Use direct write, NOT read-modify-write (|=).
     *   CORRECT: tim->EGR.ALL = (1U << 0U);
     *   WRONG:   tim->EGR.ALL |= (1U << 0U);
     *
     * TRAP 1 (T-C08): EGR.UG fires an update event which sets SR.UIF.
     * If NVIC is armed and UIF is not cleared here, the very first
     * TIM_Start produces an immediate spurious ISR before the first
     * real period expires. Clear UIF immediately after writing EGR.UG.
     *
     * TRAP 5: PSC shadow loads on UG event. Do not readback-verify PSC
     * after this — shadow register may differ from preload register.
     * ------------------------------------------------------- */
    tim->EGR.ALL = (1U << 0U);   /* UG bit — direct write, NOT |= */

    /* Clear SR.UIF immediately — rc_w0: write 0 to clear.
     * TRAP 2: Writing 1 to UIF has NO effect.
     * Use &= ~mask to clear only UIF, preserving other flags. */
    tim->SR.ALL &= ~(1U << 0U);

    /* Debug breadcrumb: EGR.UG fired, SR.UIF cleared */
    dbg_tim_stage[idx] = 4U;

    /* -------------------------------------------------------
     * T-C09: Enable UIE in DIER (update interrupt enable)
     *        Bit 0 of DIER = UIE.
     * ------------------------------------------------------- */
    tim->DIER.ALL |= (1U << 0U);

    /* Debug breadcrumb: UIE enabled */
    dbg_tim_stage[idx] = 5U;

    /* -------------------------------------------------------
     * T-C10: Latch callback and ctx into static arrays
     *        callback may be NULL — that is valid (no-op ISR).
     * ------------------------------------------------------- */
    tim_callback[idx] = cfg->callback;
    tim_ctx[idx]      = cfg->ctx;

    /* -------------------------------------------------------
     * T-C11: Mark initialized
     * ------------------------------------------------------- */
    tim_initialized[idx] = 1U;

    /* -------------------------------------------------------
     * T-C12: Debug breadcrumb — Init SUCCESS
     * ------------------------------------------------------- */
    dbg_tim_stage[idx] = 99U;

    return TIM_OK;
}

/*
 * TIM_IC_Init — Initialize a general purpose timer channel for Input Capture.
 */
TIM_Error_t TIM_IC_Init(const TIM_IC_Config_t *cfg)
{
    volatile TIM_REGS_t *tim;
    uint32_t             idx;
    uint32_t             ch_idx;
    uint32_t             polarity_bits;
    uint32_t             mask;

    if (cfg == NULL)
    {
        return TIM_ERROR_INVALID_PARAM;
    }

    tim = get_tim_regs(cfg->id);
    if (tim == NULL)
    {
        return TIM_ERROR_INVALID_TIM;
    }

    idx = (uint32_t)cfg->id;
    ch_idx = (uint32_t)cfg->channel;

    /* Disable the channel first (CCxE = 0) */
    tim->CCER.ALL &= ~(1U << (ch_idx * 4U));

    /* Configure CCMRx for Input Capture, mapped to TIx */
    if (cfg->channel == TIM_CH_1)
    {
        tim->CCMR1.ALL &= ~(3U << 0U);
        tim->CCMR1.ALL |=  (1U << 0U);
    }
    else if (cfg->channel == TIM_CH_2)
    {
        tim->CCMR1.ALL &= ~(3U << 8U);
        tim->CCMR1.ALL |=  (1U << 8U);
    }
    else if (cfg->channel == TIM_CH_3)
    {
        tim->CCMR2.ALL &= ~(3U << 0U);
        tim->CCMR2.ALL |=  (1U << 0U);
    }
    else if (cfg->channel == TIM_CH_4)
    {
        tim->CCMR2.ALL &= ~(3U << 8U);
        tim->CCMR2.ALL |=  (1U << 8U);
    }

    /* Set polarity (CCxP / CCxNP bits) */
    polarity_bits = 0U;
    if (cfg->polarity == TIM_IC_POLARITY_FALLING)
    {
        polarity_bits = (1U << 1U);
    }
    else if (cfg->polarity == TIM_IC_POLARITY_BOTH)
    {
        polarity_bits = (1U << 1U) | (1U << 3U);
    }

    mask = (1U << (ch_idx * 4U + 1U)) | (1U << (ch_idx * 4U + 3U));
    tim->CCER.ALL &= ~mask;
    tim->CCER.ALL |= (polarity_bits << (ch_idx * 4U));

    /* Enable the channel (CCxE = 1) */
    tim->CCER.ALL |= (1U << (ch_idx * 4U));

    /* Enable the interrupt for the channel (CCxIE) in DIER */
    tim->DIER.ALL |= (1U << (ch_idx + 1U));

    /* Store callback */
    tim_ic_callback[idx][ch_idx] = cfg->callback;
    tim_ic_ctx[idx][ch_idx]      = cfg->ctx;

    return TIM_OK;
}

/*
 * TIM_IC_GetCapture — Read the captured value from a channel.
 */
TIM_Error_t TIM_IC_GetCapture(TIM_Id_t id, TIM_Channel_t channel, uint32_t *val)
{
    volatile TIM_REGS_t *tim = get_tim_regs(id);

    if (tim == NULL || val == NULL)
    {
        return TIM_ERROR_INVALID_PARAM;
    }

    if (channel == TIM_CH_1)
    {
        *val = tim->CCR1.ALL;
    }
    else if (channel == TIM_CH_2)
    {
        *val = tim->CCR2.ALL;
    }
    else if (channel == TIM_CH_3)
    {
        *val = tim->CCR3.ALL;
    }
    else if (channel == TIM_CH_4)
    {
        *val = tim->CCR4.ALL;
    }
    else
    {
        return TIM_ERROR_INVALID_PARAM;
    }

    return TIM_OK;
}

/*
 * TIM_DeInit — Deinitialize a timer, restoring it to reset state.
 *
 * Ordered teardown: stop counter → disable interrupt → disable clock
 * → clear all software state.
 */
TIM_Error_t TIM_DeInit(TIM_Id_t id)
{
    volatile TIM_REGS_t *tim;
    RCC_Peripheral_t     rcc_periph;
    uint32_t             idx;

    /* -------------------------------------------------------
     * 1) Validate id
     * ------------------------------------------------------- */
    tim = get_tim_regs(id);
    if (tim == NULL)
    {
        return TIM_ERROR_INVALID_TIM;
    }

    idx = (uint32_t)id;

    /* -------------------------------------------------------
     * 2) Check initialized — return error if not
     * ------------------------------------------------------- */
    if (tim_initialized[idx] == 0U)
    {
        return TIM_ERROR_NOT_INIT;
    }

    /* -------------------------------------------------------
     * 3) Clear CR1.CEN — stop counter
     * ------------------------------------------------------- */
    tim->CR1.ALL &= ~(1U << 0U);

    /* -------------------------------------------------------
     * 4) Clear DIER.UIE — disable update interrupt
     * ------------------------------------------------------- */
    tim->DIER.ALL &= ~(1U << 0U);

    /* -------------------------------------------------------
     * 5) Clear SR.UIF — prevent pending interrupt after NVIC disable
     *    rc_w0: write 0 to clear (TRAP 2).
     * ------------------------------------------------------- */
    tim->SR.ALL &= ~(1U << 0U);

    /* -------------------------------------------------------
     * 6) Disable RCC clock
     * ------------------------------------------------------- */
    rcc_periph = get_rcc_periph(id);
    if ((uint8_t)rcc_periph != 0xFFU)
    {
        (void)RCC_DisablePeripheralClock(rcc_periph);
    }

    /* -------------------------------------------------------
     * 7) Clear all software state
     * ------------------------------------------------------- */
    tim_callback[idx]    = NULL;
    tim_ctx[idx]         = NULL;
    tim_initialized[idx] = 0U;
    tim_running[idx]     = 0U;

    /* -------------------------------------------------------
     * 8) Reset debug breadcrumb
     * ------------------------------------------------------- */
    dbg_tim_stage[idx] = 0U;

    return TIM_OK;
}

/*
 * TIM_Start — Start the timer counter (set CR1.CEN = 1).
 */
TIM_Error_t TIM_Start(TIM_Id_t id)
{
    volatile TIM_REGS_t *tim;
    uint32_t             idx;

    /* -------------------------------------------------------
     * 1) Validate id
     * ------------------------------------------------------- */
    tim = get_tim_regs(id);
    if (tim == NULL)
    {
        return TIM_ERROR_INVALID_TIM;
    }

    idx = (uint32_t)id;

    /* -------------------------------------------------------
     * 2) Check initialized
     * ------------------------------------------------------- */
    if (tim_initialized[idx] == 0U)
    {
        return TIM_ERROR_NOT_INIT;
    }

    /* -------------------------------------------------------
     * 3) Set CR1.CEN = 1
     * ------------------------------------------------------- */
    tim->CR1.ALL |= (1U << 0U);

    /* -------------------------------------------------------
     * 4) Update software running flag
     * ------------------------------------------------------- */
    tim_running[idx] = 1U;

    return TIM_OK;
}

/*
 * TIM_Stop — Stop the timer counter (clear CR1.CEN = 0).
 *
 * Does NOT clear the counter value — CNT is preserved.
 * To reset the counter, call TIM_Reset after TIM_Stop.
 */
TIM_Error_t TIM_Stop(TIM_Id_t id)
{
    volatile TIM_REGS_t *tim;
    uint32_t             idx;

    /* -------------------------------------------------------
     * 1) Validate id
     * ------------------------------------------------------- */
    tim = get_tim_regs(id);
    if (tim == NULL)
    {
        return TIM_ERROR_INVALID_TIM;
    }

    idx = (uint32_t)id;

    /* -------------------------------------------------------
     * 2) Check initialized
     * ------------------------------------------------------- */
    if (tim_initialized[idx] == 0U)
    {
        return TIM_ERROR_NOT_INIT;
    }

    /* -------------------------------------------------------
     * 3) Clear CR1.CEN = 0
     * ------------------------------------------------------- */
    tim->CR1.ALL &= ~(1U << 0U);

    /* -------------------------------------------------------
     * 4) Update software running flag
     * ------------------------------------------------------- */
    tim_running[idx] = 0U;

    return TIM_OK;
}

/*
 * TIM_Reset — Force counter reload via software update event.
 *
 * Writes EGR.UG = 1 to force an update event, reloading PSC and ARR
 * shadow registers and resetting the counter to 0 (or ARR in downcount).
 * Clears SR.UIF after to prevent spurious ISR.
 */
TIM_Error_t TIM_Reset(TIM_Id_t id)
{
    volatile TIM_REGS_t *tim;
    uint32_t             idx;

    /* -------------------------------------------------------
     * 1) Validate id
     * ------------------------------------------------------- */
    tim = get_tim_regs(id);
    if (tim == NULL)
    {
        return TIM_ERROR_INVALID_TIM;
    }

    idx = (uint32_t)id;

    /* -------------------------------------------------------
     * 2) Check initialized
     * ------------------------------------------------------- */
    if (tim_initialized[idx] == 0U)
    {
        return TIM_ERROR_NOT_INIT;
    }

    /* -------------------------------------------------------
     * 3) Write EGR.UG = 1 — direct write, NOT |=
     *    EGR is write-only (T-I05 / TRAP 4). Reading returns 0x0000.
     * ------------------------------------------------------- */
    tim->EGR.ALL = (1U << 0U);

    /* -------------------------------------------------------
     * 4) Clear SR.UIF immediately — rc_w0 (TRAP 2)
     *    UG event sets UIF. If NVIC is enabled, this would fire
     *    a spurious ISR. Clear it so TIM_Reset is side-effect-free.
     * ------------------------------------------------------- */
    tim->SR.ALL &= ~(1U << 0U);

    return TIM_OK;
}

/* ========================================================= */
/* ================= Counter Access ======================== */
/* ========================================================= */

/*
 * TIM_GetCounter — Read the current counter value (CNT register).
 *
 * T-I03: Out-param pattern — returning 0 by value would be ambiguous
 * between "counter is zero" and "invalid ID".
 */
TIM_Error_t TIM_GetCounter(TIM_Id_t id, uint32_t *count)
{
    volatile TIM_REGS_t *tim;
    uint32_t             idx;

    /* -------------------------------------------------------
     * 1) NULL check on out-param
     * ------------------------------------------------------- */
    if (count == NULL)
    {
        return TIM_ERROR_INVALID_PARAM;
    }

    /* -------------------------------------------------------
     * 2) Validate id
     * ------------------------------------------------------- */
    tim = get_tim_regs(id);
    if (tim == NULL)
    {
        return TIM_ERROR_INVALID_TIM;
    }

    idx = (uint32_t)id;

    /* -------------------------------------------------------
     * 3) Check initialized
     * ------------------------------------------------------- */
    if (tim_initialized[idx] == 0U)
    {
        return TIM_ERROR_NOT_INIT;
    }

    /* -------------------------------------------------------
     * 4) Read CNT register
     *    T-03: 32-bit on TIM2/TIM5, upper 16 bits zero on TIM3/TIM4.
     * ------------------------------------------------------- */
    *count = tim->CNT.ALL;

    return TIM_OK;
}

/*
 * TIM_SetCounter — Write a value directly to the counter register.
 *
 * On TIM3/TIM4 (16-bit timers), upper 16 bits are reserved.
 * Caller is responsible for keeping the value within 16 bits on those timers.
 * This driver does NOT mask the value — it writes as-is.
 */
TIM_Error_t TIM_SetCounter(TIM_Id_t id, uint32_t value)
{
    volatile TIM_REGS_t *tim;
    uint32_t             idx;

    /* -------------------------------------------------------
     * 1) Validate id
     * ------------------------------------------------------- */
    tim = get_tim_regs(id);
    if (tim == NULL)
    {
        return TIM_ERROR_INVALID_TIM;
    }

    idx = (uint32_t)id;

    /* -------------------------------------------------------
     * 2) Check initialized
     * ------------------------------------------------------- */
    if (tim_initialized[idx] == 0U)
    {
        return TIM_ERROR_NOT_INIT;
    }

    /* -------------------------------------------------------
     * 3) Write CNT register
     * ------------------------------------------------------- */
    tim->CNT.ALL = value;

    return TIM_OK;
}

/* ========================================================= */
/* ================= Prescaler & Period ==================== */
/* ========================================================= */

/*
 * TIM_SetPrescaler — Update the prescaler register.
 *
 * The new value takes effect on the next update event (counter overflow
 * or EGR.UG). To force immediate application, call TIM_Reset after.
 *
 * T-I01: Pass the raw register value. Hardware divides by (psc+1).
 * TRAP 5: Do not readback-verify PSC — shadow may differ from preload.
 */
TIM_Error_t TIM_SetPrescaler(TIM_Id_t id, uint32_t psc)
{
    volatile TIM_REGS_t *tim;
    uint32_t             idx;

    /* -------------------------------------------------------
     * 1) Validate id
     * ------------------------------------------------------- */
    tim = get_tim_regs(id);
    if (tim == NULL)
    {
        return TIM_ERROR_INVALID_TIM;
    }

    idx = (uint32_t)id;

    /* -------------------------------------------------------
     * 2) Check initialized
     * ------------------------------------------------------- */
    if (tim_initialized[idx] == 0U)
    {
        return TIM_ERROR_NOT_INIT;
    }

    /* -------------------------------------------------------
     * 3) Write PSC register
     * ------------------------------------------------------- */
    tim->PSC.ALL = psc;

    return TIM_OK;
}

/*
 * TIM_SetPeriod — Update the auto-reload register.
 *
 * With ARPE=1 (recommended), the shadow register loads on the next
 * update event. With ARPE=0, loads immediately — may cause a glitch
 * if the counter value is already greater than the new ARR.
 *
 * T-I01: Pass the raw register value. Hardware counts (arr+1) ticks.
 */
TIM_Error_t TIM_SetPeriod(TIM_Id_t id, uint32_t arr)
{
    volatile TIM_REGS_t *tim;
    uint32_t             idx;

    /* -------------------------------------------------------
     * 1) Validate id
     * ------------------------------------------------------- */
    tim = get_tim_regs(id);
    if (tim == NULL)
    {
        return TIM_ERROR_INVALID_TIM;
    }

    idx = (uint32_t)id;

    /* -------------------------------------------------------
     * 2) Check initialized
     * ------------------------------------------------------- */
    if (tim_initialized[idx] == 0U)
    {
        return TIM_ERROR_NOT_INIT;
    }

    /* -------------------------------------------------------
     * 3) Write ARR register
     * ------------------------------------------------------- */
    tim->ARR.ALL = arr;

    return TIM_OK;
}

/* ========================================================= */
/* ================= Status Query ========================== */
/* ========================================================= */

/*
 * TIM_IsRunning — Check whether the timer counter is active.
 *
 * Reads CR1.CEN from the hardware register (ground truth),
 * not from the tim_running[] software cache.
 *
 * T-I03: Out-param pattern — 0 returned by value would be ambiguous
 * between "not running" and "invalid ID".
 */
TIM_Error_t TIM_IsRunning(TIM_Id_t id, uint8_t *state)
{
    volatile TIM_REGS_t *tim;
    uint32_t             idx;

    /* -------------------------------------------------------
     * 1) NULL check on out-param
     * ------------------------------------------------------- */
    if (state == NULL)
    {
        return TIM_ERROR_INVALID_PARAM;
    }

    /* -------------------------------------------------------
     * 2) Validate id
     * ------------------------------------------------------- */
    tim = get_tim_regs(id);
    if (tim == NULL)
    {
        return TIM_ERROR_INVALID_TIM;
    }

    idx = (uint32_t)id;

    /* -------------------------------------------------------
     * 3) Check initialized
     * ------------------------------------------------------- */
    if (tim_initialized[idx] == 0U)
    {
        return TIM_ERROR_NOT_INIT;
    }

    /* -------------------------------------------------------
     * 4) Read CEN bit from hardware — register is ground truth
     * ------------------------------------------------------- */
    *state = (uint8_t)((tim->CR1.ALL >> 0U) & 1U);

    return TIM_OK;
}

/*
 * TIM_GetUpdateFlag — Read SR.UIF without clearing it.
 *
 * Does NOT clear the flag. Caller must call TIM_ClearUpdateFlag
 * explicitly if clearing is desired.
 */
TIM_Error_t TIM_GetUpdateFlag(TIM_Id_t id, uint8_t *state)
{
    volatile TIM_REGS_t *tim;
    uint32_t             idx;

    /* -------------------------------------------------------
     * 1) NULL check on out-param
     * ------------------------------------------------------- */
    if (state == NULL)
    {
        return TIM_ERROR_INVALID_PARAM;
    }

    /* -------------------------------------------------------
     * 2) Validate id
     * ------------------------------------------------------- */
    tim = get_tim_regs(id);
    if (tim == NULL)
    {
        return TIM_ERROR_INVALID_TIM;
    }

    idx = (uint32_t)id;

    /* -------------------------------------------------------
     * 3) Check initialized
     * ------------------------------------------------------- */
    if (tim_initialized[idx] == 0U)
    {
        return TIM_ERROR_NOT_INIT;
    }

    /* -------------------------------------------------------
     * 4) Read UIF bit from SR register
     * ------------------------------------------------------- */
    *state = (uint8_t)((tim->SR.ALL >> 0U) & 1U);

    return TIM_OK;
}

/*
 * TIM_ClearUpdateFlag — Clear SR.UIF.
 *
 * T-I04 / TRAP 2: SR.UIF is rc_w0 — read/clear-write-0.
 *   CORRECT:  tim->SR.ALL &= ~(1U << 0U);   ← clears only UIF
 *   WRONG:    tim->SR.ALL |=  (1U << 0U);    ← no-op, UIF stays set
 *   WRONG:    tim->SR.ALL  =  0U;            ← clears ALL flags, may
 *             discard CC1IF–CC4IF capture events silently.
 *
 * CRITICAL: If you forget to clear UIF inside the ISR, the update
 *           interrupt re-enters immediately after return.
 */
TIM_Error_t TIM_ClearUpdateFlag(TIM_Id_t id)
{
    volatile TIM_REGS_t *tim;
    uint32_t             idx;

    /* -------------------------------------------------------
     * 1) Validate id
     * ------------------------------------------------------- */
    tim = get_tim_regs(id);
    if (tim == NULL)
    {
        return TIM_ERROR_INVALID_TIM;
    }

    idx = (uint32_t)id;

    /* -------------------------------------------------------
     * 2) Check initialized
     * ------------------------------------------------------- */
    if (tim_initialized[idx] == 0U)
    {
        return TIM_ERROR_NOT_INIT;
    }

    /* -------------------------------------------------------
     * 3) Clear UIF — rc_w0: write 0 to bit 0 to clear
     * ------------------------------------------------------- */
    tim->SR.ALL &= ~(1U << 0U);

    return TIM_OK;
}

/* ========================================================= */
/* ================= Interrupt Control ===================== */
/* ========================================================= */

/*
 * TIM_EnableUpdateIRQ — Enable the update interrupt (DIER.UIE = 1).
 *
 * T-I05: This enables the interrupt at the timer peripheral level only.
 * The NVIC line must be enabled separately by the caller.
 */
TIM_Error_t TIM_EnableUpdateIRQ(TIM_Id_t id)
{
    volatile TIM_REGS_t *tim;
    uint32_t             idx;

    /* -------------------------------------------------------
     * 1) Validate id
     * ------------------------------------------------------- */
    tim = get_tim_regs(id);
    if (tim == NULL)
    {
        return TIM_ERROR_INVALID_TIM;
    }

    idx = (uint32_t)id;

    /* -------------------------------------------------------
     * 2) Check initialized
     * ------------------------------------------------------- */
    if (tim_initialized[idx] == 0U)
    {
        return TIM_ERROR_NOT_INIT;
    }

    /* -------------------------------------------------------
     * 3) Set DIER.UIE (bit 0)
     * ------------------------------------------------------- */
    tim->DIER.ALL |= (1U << 0U);

    return TIM_OK;
}

/*
 * TIM_DisableUpdateIRQ — Disable the update interrupt (DIER.UIE = 0).
 */
TIM_Error_t TIM_DisableUpdateIRQ(TIM_Id_t id)
{
    volatile TIM_REGS_t *tim;
    uint32_t             idx;

    /* -------------------------------------------------------
     * 1) Validate id
     * ------------------------------------------------------- */
    tim = get_tim_regs(id);
    if (tim == NULL)
    {
        return TIM_ERROR_INVALID_TIM;
    }

    idx = (uint32_t)id;

    /* -------------------------------------------------------
     * 2) Check initialized
     * ------------------------------------------------------- */
    if (tim_initialized[idx] == 0U)
    {
        return TIM_ERROR_NOT_INIT;
    }

    /* -------------------------------------------------------
     * 3) Clear DIER.UIE (bit 0)
     * ------------------------------------------------------- */
    tim->DIER.ALL &= ~(1U << 0U);

    return TIM_OK;
}

/* ========================================================= */
/* ================= Callback Registration ================= */
/* ========================================================= */

/*
 * TIM_RegisterCallback — Register or replace the update event callback.
 *
 * cb == NULL is valid — disables the callback (ISR becomes a no-op
 * after clearing UIF).
 *
 * Safe to call while the timer is running — single aligned pointer
 * writes are atomic on Cortex-M4 (no critical section needed).
 */
TIM_Error_t TIM_RegisterCallback(TIM_Id_t id, TIM_Callback_t cb, void *ctx)
{
    volatile TIM_REGS_t *tim;
    uint32_t             idx;

    /* -------------------------------------------------------
     * 1) Validate id (get_tim_regs also serves as range check)
     * ------------------------------------------------------- */
    tim = get_tim_regs(id);
    if (tim == NULL)
    {
        return TIM_ERROR_INVALID_TIM;
    }

    idx = (uint32_t)id;

    /* -------------------------------------------------------
     * 2) Check initialized
     * ------------------------------------------------------- */
    if (tim_initialized[idx] == 0U)
    {
        return TIM_ERROR_NOT_INIT;
    }

    /* -------------------------------------------------------
     * 3) Latch callback and context
     *    Write ctx first so callback never sees stale ctx.
     * ------------------------------------------------------- */
    tim_ctx[idx]      = ctx;
    tim_callback[idx] = cb;

    /* Suppress unused variable warning — tim was used only for
     * id validation via get_tim_regs, not for register access. */
    (void)tim;

    return TIM_OK;
}

/* ========================================================= */
/* ================= IRQ Handlers ========================== */
/* ========================================================= */

/*
 * MANDATORY ISR SEQUENCE (Rule 7):
 *   a) Clear SR.UIF FIRST — rc_w0: write 0 to clear (TRAP 2, TRAP 3)
 *      Must happen before callback to avoid losing period N+1 event
 *      if callback takes non-zero time and another period expires.
 *   b) Debug breadcrumb
 *   c) Call callback if non-NULL, pass ctx
 *   d) No other logic. ISR body < 20 lines.
 *
 * These names match the vector table entries in the startup file
 * (startup_stm32f401xc.s or equivalent).
 */

void TIM2_IRQHandler(void)
{
    /* -------------------------------------------------------
     * 1) Clear UIF FIRST — rc_w0: write 0 to clear.
     *    Must happen before callback to avoid re-entry if
     *    callback takes long and another period expires.
     *    Writing 1 to UIF has no effect (rc_w0 field, T-I04).
     * ------------------------------------------------------- */
    TIM2->SR.ALL &= ~(1U << 0U);

    /* -------------------------------------------------------
     * 2) Debug breadcrumb — readable in Watch panel
     * ------------------------------------------------------- */
    dbg_tim_stage[0U] = 0xA0U;

    /* -------------------------------------------------------
     * 3) Invoke user callback if registered
     * ------------------------------------------------------- */
    if (tim_callback[0U] != NULL)
    {
        tim_callback[0U](tim_ctx[0U]);
    }
}

void TIM3_IRQHandler(void)
{
    dbg_tim_stage[1U] = 0xA1U;

    /* Check Update Event */
    if ((TIM3->SR.ALL & (1U << 0U)) != 0U && (TIM3->DIER.ALL & (1U << 0U)) != 0U)
    {
        TIM3->SR.ALL &= ~(1U << 0U);
        if (tim_callback[1U] != NULL)
        {
            tim_callback[1U](tim_ctx[1U]);
        }
    }

    /* Check CC1 */
    if ((TIM3->SR.ALL & (1U << 1U)) != 0U && (TIM3->DIER.ALL & (1U << 1U)) != 0U)
    {
        TIM3->SR.ALL &= ~(1U << 1U);
        if (tim_ic_callback[1U][0U] != NULL)
        {
            tim_ic_callback[1U][0U](tim_ic_ctx[1U][0U]);
        }
    }

    /* Check CC2 */
    if ((TIM3->SR.ALL & (1U << 2U)) != 0U && (TIM3->DIER.ALL & (1U << 2U)) != 0U)
    {
        TIM3->SR.ALL &= ~(1U << 2U);
        if (tim_ic_callback[1U][1U] != NULL)
        {
            tim_ic_callback[1U][1U](tim_ic_ctx[1U][1U]);
        }
    }

    /* Check CC3 */
    if ((TIM3->SR.ALL & (1U << 3U)) != 0U && (TIM3->DIER.ALL & (1U << 3U)) != 0U)
    {
        TIM3->SR.ALL &= ~(1U << 3U);
        if (tim_ic_callback[1U][2U] != NULL)
        {
            tim_ic_callback[1U][2U](tim_ic_ctx[1U][2U]);
        }
    }

    /* Check CC4 */
    if ((TIM3->SR.ALL & (1U << 4U)) != 0U && (TIM3->DIER.ALL & (1U << 4U)) != 0U)
    {
        TIM3->SR.ALL &= ~(1U << 4U);
        if (tim_ic_callback[1U][3U] != NULL)
        {
            tim_ic_callback[1U][3U](tim_ic_ctx[1U][3U]);
        }
    }
}

void TIM4_IRQHandler(void)
{
    /* 1) Clear UIF FIRST — rc_w0 */
    TIM4->SR.ALL &= ~(1U << 0U);

    /* 2) Debug breadcrumb */
    dbg_tim_stage[2U] = 0xA2U;

    /* 3) Invoke user callback if registered */
    if (tim_callback[2U] != NULL)
    {
        tim_callback[2U](tim_ctx[2U]);
    }
}

void TIM5_IRQHandler(void)
{
    /* 1) Clear UIF FIRST — rc_w0 */
    TIM5->SR.ALL &= ~(1U << 0U);

    /* 2) Debug breadcrumb */
    dbg_tim_stage[3U] = 0xA3U;

    /* 3) Invoke user callback if registered */
    if (tim_callback[3U] != NULL)
    {
        tim_callback[3U](tim_ctx[3U]);
    }
}