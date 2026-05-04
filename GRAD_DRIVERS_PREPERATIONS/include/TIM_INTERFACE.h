/*****************************************************************************
 * TIM_INTERFACE.h — Public API for General Purpose Timer Driver (TIM2–TIM5)
 *                   STM32F401CC, Reference Manual RM0368
 *
 * Primary use case: TIM2 at 100 Hz to trigger MPU6050 DMA reads
 * for road surface data collection (bare-metal, no FreeRTOS).
 *
 * ═══════════════════════════════════════════════════════════════════════
 * CLOCK CONTEXT
 * ═══════════════════════════════════════════════════════════════════════
 *
 * System clock : 84 MHz (HSI PLL, configured by RCC_INIT_84MHz_HSI)
 * APB1 prescaler : /2 → APB1 bus clock = 42 MHz
 * Timer clock  : 2 × APB1 = 84 MHz (hardware doubles when APB1 ≠ /1)
 *
 * 100 Hz derivation:
 *   PSC = 8399  → tick rate = 84,000,000 / (8399+1) = 10,000 Hz
 *   ARR = 99    → period    = 10,000 / (99+1)       = 100 Hz   ✓
 *
 * T-I01: PSC and ARR store (value), not (value+1).
 *        Hardware adds 1 internally: actual period = (PSC+1)(ARR+1) / TIM_CLK.
 *        TIM_PSC_100HZ = 8399U means PSC register = 8399, actual divisor = 8400.
 *        Do not subtract 1 before writing — TIM_Init writes the value directly.
 *
 * T-I02: EGR.UG must be fired after writing PSC and ARR, before CEN.
 *        Without it, the first period uses the reset value of the shadow
 *        registers, not the configured PSC/ARR. This causes a one-shot
 *        timing error on the very first tick only — hard to catch in testing.
 *
 * T-I03: Out-param pattern on all boolean queries — 0U return is ambiguous
 *        between "condition false" and "invalid ID". Same fix as I2C_IsBusy,
 *        UART_IsTxEmpty in this codebase.
 *
 * T-I04: SR.UIF is rc_w0: hardware sets it, software clears by writing 0.
 *        Writing 1 to UIF has no effect. Use:
 *          TIM2->SR.ALL &= ~(1U << 0U);    ← correct
 *          TIM2->SR.ALL = 0U;               ← also acceptable (clears all flags)
 *        NOT: TIM2->SR.ALL |= (1U << 0U);  ← this is a no-op, UIF stays set,
 *                                             ISR re-enters immediately.
 *
 * T-I05: TIM_Init does NOT enable the NVIC line. Caller must call
 *        NVIC_EnableIRQ(TIM2) after TIM_Init. This keeps the timer driver
 *        independent of the NVIC driver and avoids double-enable bugs.
 *
 *****************************************************************************/

#ifndef TIM_INTERFACE_H
#define TIM_INTERFACE_H

#include "STD_TYPES.h"

/* ========================================================= */
/* ================= Timer Clock Defines =================== */
/* ========================================================= */

/*
 * Timer clock derivation — 84 MHz system clock, APB1 /2
 * APB1 = 42 MHz; timer clock = 2 × 42 MHz = 84 MHz
 * Target: 100 Hz update event
 *
 * PSC = 8399  → tick rate = 84,000,000 / (8399 + 1) = 10,000 Hz
 * ARR = 99    → period    = 10,000 / (99 + 1)       = 100 Hz
 *
 * T-I01: These are the register values — hardware adds 1 internally.
 *        Do NOT subtract 1 before passing to TIM_Init.
 */
#define TIM_CLOCK_HZ      84000000UL
#define TIM_PSC_100HZ     8399U
#define TIM_ARR_100HZ     99U

/* ========================================================= */
/* ================= Error Codes =========================== */
/* ========================================================= */

/*
 * TIM_Error_t — Function return codes.
 *
 * T-I06: Every enumerator has an explicit hex value per MISRA Rule 8.12.
 *        No implicit increment anywhere.
 */
typedef enum
{
    TIM_OK                  = 0x00U,  /* Operation successful                    */
    TIM_ERROR_INVALID_PARAM = 0x01U,  /* NULL pointer or out-of-range value      */
    TIM_ERROR_INVALID_TIM   = 0x02U,  /* ID is not TIM2/3/4/5                    */
    TIM_ERROR_BUSY          = 0x03U,  /* Timer already running, Init rejected    */
    TIM_ERROR_NOT_INIT      = 0x04U,  /* TIM_Init not called before this function*/
    TIM_ERROR_TIMEOUT       = 0x05U   /* Reserved — update event did not fire    */
} TIM_Error_t;

/* ========================================================= */
/* ================= Timer Instance ID ===================== */
/* ========================================================= */

/*
 * TIM_Id_t — Identifies which timer peripheral to operate on.
 * Only TIM2–TIM5 (general purpose, APB1 bus) are supported.
 */
typedef enum
{
    TIM_ID_2 = 0U,   /* TIM2 — 32-bit counter (primary 100 Hz timer) */
    TIM_ID_3 = 1U,   /* TIM3 — 16-bit counter                        */
    TIM_ID_4 = 2U,   /* TIM4 — 16-bit counter                        */
    TIM_ID_5 = 3U    /* TIM5 — 32-bit counter                        */
} TIM_Id_t;

/* ========================================================= */
/* ================= Counter Mode ========================== */
/* ========================================================= */

/*
 * TIM_Mode_t — Counter direction.
 * Center-aligned modes omitted — not needed for 100 Hz tick use case.
 */
typedef enum
{
    TIM_MODE_UP   = 0U,  /* Upcounting   (CR1.DIR=0, CR1.CMS=00) */
    TIM_MODE_DOWN = 1U   /* Downcounting (CR1.DIR=1, CR1.CMS=00) */
} TIM_Mode_t;

/* ========================================================= */
/* ================= Auto-Reload Preload =================== */
/* ========================================================= */

/*
 * TIM_AutoReloadPreload_t — Controls ARR double-buffering via CR1.ARPE.
 *
 * T-I07: With ARPE enabled, writes to ARR go to the preload register and
 *        transfer to the shadow register on the next update event. This
 *        prevents glitches when changing the period on a running timer.
 *        Recommended: TIM_ARPE_ENABLE for production use.
 */
typedef enum
{
    TIM_ARPE_DISABLE = 0U,  /* ARR not buffered — loads immediately     */
    TIM_ARPE_ENABLE  = 1U   /* ARR buffered — loads on update event     */
} TIM_AutoReloadPreload_t;

/* ========================================================= */
/* ================= Callback Type ========================= */
/* ========================================================= */

/*
 * TIM_Callback_t — ISR notification callback.
 *
 * Fired from TIMx_IRQHandler on every update event (e.g. 100 Hz for TIM2).
 * MUST be ISR-safe: no blocking, no printf, no HAL calls.
 *
 * ctx: opaque pointer passed at registration time, forwarded to callback.
 *      Allows the same callback function to serve multiple timer instances
 *      by distinguishing context.
 *
 * T-I08: Named typedef, not anonymous — required by MISRA Rule 8.12
 *        and project convention for callback pointer types.
 */
typedef void (*TIM_Callback_t)(void *ctx);

/* ========================================================= */
/* ================= Configuration Struct ================== */
/* ========================================================= */

/*
 * TIM_Config_t — Initialization parameters for TIM_Init.
 *
 * Typical 100 Hz configuration:
 *   TIM_Config_t cfg = {
 *       .id        = TIM_ID_2,
 *       .prescaler = TIM_PSC_100HZ,     // 8399U
 *       .period    = TIM_ARR_100HZ,     // 99U
 *       .mode      = TIM_MODE_UP,
 *       .arpe      = TIM_ARPE_ENABLE,
 *       .callback  = MyUpdateCallback,
 *       .ctx       = (void *)0
 *   };
 *
 * T-I01: prescaler and period are the raw register values.
 *        Hardware divides by (prescaler+1) and (period+1).
 *        Pass 8399U, not 8400U.
 */
typedef struct
{
    TIM_Id_t                  id;         /* Timer instance (TIM_ID_2..5)       */
    uint32_t                  prescaler;  /* Written to PSC register (raw value)*/
    uint32_t                  period;     /* Written to ARR register (raw value)*/
    TIM_Mode_t                mode;       /* Upcounting or downcounting         */
    TIM_AutoReloadPreload_t   arpe;       /* ARR preload — TIM_ARPE_ENABLE rec */
    TIM_Callback_t            callback;   /* Update event ISR callback (or NULL)*/
    void                     *ctx;        /* Opaque pointer passed to callback  */
} TIM_Config_t;

/* ========================================================= */
/* ================= Initialization & Control ============== */
/* ========================================================= */

/*
 * TIM_Init — Initialize a general purpose timer.
 *
 * Actions performed:
 *   1. Enables TIM clock via RCC (caller must NOT pre-enable it)
 *   2. Writes PSC, ARR, CR1 (mode + ARPE)
 *   3. Fires EGR.UG to force shadow register load (T-I02)
 *   4. Clears SR.UIF (prevent spurious ISR from EGR.UG)
 *   5. Enables UIE in DIER
 *   6. Stores callback and context pointer
 *
 * Does NOT start the counter — call TIM_Start separately.
 *
 * T-I05: Does NOT enable the NVIC line. Caller must call
 *        NVIC_EnableIRQ after TIM_Init. This keeps the timer driver
 *        independent of the NVIC driver and avoids double-enable bugs.
 *
 * cfg     : pointer to configuration struct (must not be NULL)
 * Returns : TIM_OK on success
 *           TIM_ERROR_INVALID_PARAM if cfg is NULL
 *           TIM_ERROR_INVALID_TIM   if cfg->id is out of range
 *           TIM_ERROR_BUSY          if timer is already running (CEN=1)
 */
TIM_Error_t TIM_Init(const TIM_Config_t *cfg);

/*
 * TIM_DeInit — Deinitialize a timer, restoring it to reset state.
 *
 * Actions performed:
 *   1. Stops counter (CEN=0)
 *   2. Disables update interrupt (UIE=0)
 *   3. Disables RCC clock for the timer
 *   4. Clears registered callback
 *
 * id      : timer instance
 * Returns : TIM_OK on success
 *           TIM_ERROR_INVALID_TIM if id is out of range
 *           TIM_ERROR_NOT_INIT    if TIM_Init was not called for this id
 */
TIM_Error_t TIM_DeInit(TIM_Id_t id);

/*
 * TIM_Start — Start the timer counter.
 *
 * Sets CR1.CEN = 1. Counter begins counting from its current value.
 *
 * id      : timer instance
 * Returns : TIM_OK on success
 *           TIM_ERROR_INVALID_TIM if id is out of range
 *           TIM_ERROR_NOT_INIT    if TIM_Init was not called for this id
 */
TIM_Error_t TIM_Start(TIM_Id_t id);

/*
 * TIM_Stop — Stop the timer counter.
 *
 * Clears CR1.CEN = 0. Counter value is preserved (not cleared).
 * To also reset the counter, call TIM_Reset after TIM_Stop.
 *
 * id      : timer instance
 * Returns : TIM_OK on success
 *           TIM_ERROR_INVALID_TIM if id is out of range
 *           TIM_ERROR_NOT_INIT    if TIM_Init was not called for this id
 */
TIM_Error_t TIM_Stop(TIM_Id_t id);

/*
 * TIM_Reset — Force counter reload via software update event.
 *
 * Actions performed:
 *   1. Writes EGR.UG = 1 to force update event (reloads PSC/ARR shadow)
 *   2. Clears SR.UIF after (prevent spurious ISR)
 *
 * T-I04: SR.UIF is rc_w0. Cleared by writing 0, NOT by writing 1.
 *
 * id      : timer instance
 * Returns : TIM_OK on success
 *           TIM_ERROR_INVALID_TIM if id is out of range
 *           TIM_ERROR_NOT_INIT    if TIM_Init was not called for this id
 */
TIM_Error_t TIM_Reset(TIM_Id_t id);

/* ========================================================= */
/* ================= Counter Access ======================== */
/* ========================================================= */

/*
 * TIM_GetCounter — Read the current counter value.
 *
 * Reads CNT register. On TIM3/TIM4 (16-bit timers), upper 16 bits
 * of the returned value will be zero.
 *
 * T-I03: Out-param pattern — returning the value directly would make
 *        0 ambiguous between "counter is zero" and "invalid ID".
 *
 * id      : timer instance
 * count   : out-param — receives current CNT value (must not be NULL)
 * Returns : TIM_OK on success
 *           TIM_ERROR_INVALID_PARAM if count is NULL
 *           TIM_ERROR_INVALID_TIM   if id is out of range
 *           TIM_ERROR_NOT_INIT      if TIM_Init was not called for this id
 */
TIM_Error_t TIM_GetCounter(TIM_Id_t id, uint32_t *count);

/*
 * TIM_SetCounter — Write a value directly to the counter register.
 *
 * Writes CNT register. On TIM3/TIM4 (16-bit timers), upper 16 bits
 * are reserved — caller is responsible for keeping value within 16 bits.
 *
 * id      : timer instance
 * value   : value to write to CNT
 * Returns : TIM_OK on success
 *           TIM_ERROR_INVALID_TIM if id is out of range
 *           TIM_ERROR_NOT_INIT    if TIM_Init was not called for this id
 */
TIM_Error_t TIM_SetCounter(TIM_Id_t id, uint32_t value);

/* ========================================================= */
/* ================= Prescaler & Period ==================== */
/* ========================================================= */

/*
 * TIM_SetPrescaler — Update the prescaler register.
 *
 * Writes PSC register. The new value takes effect on the next update
 * event (counter overflow/underflow or EGR.UG). To force immediate
 * application, call TIM_Reset after this function.
 *
 * T-I01: Pass the raw register value. Hardware divides by (psc+1).
 *
 * id      : timer instance
 * psc     : prescaler value (raw, not +1)
 * Returns : TIM_OK on success
 *           TIM_ERROR_INVALID_TIM if id is out of range
 *           TIM_ERROR_NOT_INIT    if TIM_Init was not called for this id
 */
TIM_Error_t TIM_SetPrescaler(TIM_Id_t id, uint32_t psc);

/*
 * TIM_SetPeriod — Update the auto-reload register.
 *
 * Writes ARR register. With ARPE enabled (recommended), the new value
 * transfers to the shadow register on the next update event.
 * With ARPE disabled, the value loads immediately.
 *
 * T-I01: Pass the raw register value. Hardware counts (arr+1) ticks.
 *
 * id      : timer instance
 * arr     : auto-reload value (raw, not +1)
 * Returns : TIM_OK on success
 *           TIM_ERROR_INVALID_TIM if id is out of range
 *           TIM_ERROR_NOT_INIT    if TIM_Init was not called for this id
 */
TIM_Error_t TIM_SetPeriod(TIM_Id_t id, uint32_t arr);

/* ========================================================= */
/* ================= Status Query ========================== */
/* ========================================================= */

/*
 * TIM_IsRunning — Check whether the timer counter is active.
 *
 * Reads CR1.CEN bit. state is set to 1U if CEN=1, 0U otherwise.
 *
 * T-I03: Out-param pattern — 0 returned by value would be ambiguous
 *        between "not running" and "invalid ID".
 *
 * id      : timer instance
 * state   : out-param — 1U if running, 0U if stopped (must not be NULL)
 * Returns : TIM_OK on success
 *           TIM_ERROR_INVALID_PARAM if state is NULL
 *           TIM_ERROR_INVALID_TIM   if id is out of range
 *           TIM_ERROR_NOT_INIT      if TIM_Init was not called for this id
 */
TIM_Error_t TIM_IsRunning(TIM_Id_t id, uint8_t *state);

/*
 * TIM_GetUpdateFlag — Read SR.UIF without clearing it.
 *
 * state is set to 1U if UIF=1 (update event occurred), 0U otherwise.
 * Does NOT clear the flag — caller must clear it explicitly via
 * TIM_ClearUpdateFlag if desired.
 *
 * T-I03: Out-param pattern for the same ambiguity reason.
 *
 * id      : timer instance
 * state   : out-param — 1U if UIF set, 0U otherwise (must not be NULL)
 * Returns : TIM_OK on success
 *           TIM_ERROR_INVALID_PARAM if state is NULL
 *           TIM_ERROR_INVALID_TIM   if id is out of range
 *           TIM_ERROR_NOT_INIT      if TIM_Init was not called for this id
 */
TIM_Error_t TIM_GetUpdateFlag(TIM_Id_t id, uint8_t *state);

/*
 * TIM_ClearUpdateFlag — Clear SR.UIF.
 *
 * T-I04: SR.UIF is rc_w0 — read/clear-write-0 type.
 *        CLEAR by writing 0 to the UIF bit position.
 *        Writing 1 to UIF has NO effect — it does NOT clear the flag.
 *
 *        Correct:   TIMx->SR.ALL &= ~(1U << 0U);
 *        Also OK:   TIMx->SR.ALL = 0U;
 *        WRONG:     TIMx->SR.ALL |= (1U << 0U);  ← no-op, ISR re-enters
 *
 * CRITICAL: If you forget to call this inside the ISR, the update
 *           interrupt re-enters immediately after return.
 *
 * id      : timer instance
 * Returns : TIM_OK on success
 *           TIM_ERROR_INVALID_TIM if id is out of range
 *           TIM_ERROR_NOT_INIT    if TIM_Init was not called for this id
 */
TIM_Error_t TIM_ClearUpdateFlag(TIM_Id_t id);

/* ========================================================= */
/* ================= Interrupt Control ===================== */
/* ========================================================= */

/*
 * TIM_EnableUpdateIRQ — Enable the update interrupt (DIER.UIE = 1).
 *
 * T-I05: This enables the interrupt at the timer peripheral level only.
 *        The NVIC line must be enabled separately by the caller.
 *
 * id      : timer instance
 * Returns : TIM_OK on success
 *           TIM_ERROR_INVALID_TIM if id is out of range
 *           TIM_ERROR_NOT_INIT    if TIM_Init was not called for this id
 */
TIM_Error_t TIM_EnableUpdateIRQ(TIM_Id_t id);

/*
 * TIM_DisableUpdateIRQ — Disable the update interrupt (DIER.UIE = 0).
 *
 * id      : timer instance
 * Returns : TIM_OK on success
 *           TIM_ERROR_INVALID_TIM if id is out of range
 *           TIM_ERROR_NOT_INIT    if TIM_Init was not called for this id
 */
TIM_Error_t TIM_DisableUpdateIRQ(TIM_Id_t id);

/* ========================================================= */
/* ================= Callback Registration ================= */
/* ========================================================= */

/*
 * TIM_RegisterCallback — Register or replace the update event callback.
 *
 * Replaces the callback that was set at TIM_Init time.
 * Passing NULL for cb disables the callback (ISR becomes a no-op after
 * clearing UIF).
 *
 * Safe to call while the timer is running — the update is atomic
 * (single pointer write on Cortex-M4 is naturally atomic).
 *
 * cb      : new callback function, or NULL to disable
 * ctx     : opaque pointer forwarded to cb on each invocation
 * id      : timer instance
 * Returns : TIM_OK on success
 *           TIM_ERROR_INVALID_TIM if id is out of range
 *           TIM_ERROR_NOT_INIT    if TIM_Init was not called for this id
 */
TIM_Error_t TIM_RegisterCallback(TIM_Id_t id, TIM_Callback_t cb, void *ctx);

/* ========================================================= */
/* ================= IRQ Handler Prototypes ================ */
/* ========================================================= */

/*
 * Timer IRQ handlers — defined in TIM.c, declared here for linker/startup
 * file visibility.
 *
 * Each handler performs:
 *   1. Reads SR.UIF — if not set, returns (spurious interrupt guard)
 *   2. Clears SR.UIF by writing 0 (rc_w0 — T-I04)
 *   3. Calls the registered callback (if non-NULL) with stored ctx pointer
 *
 * These names must match exactly the vector table entries in the startup
 * file (startup_stm32f401xc.s or equivalent).
 */
void TIM2_IRQHandler(void);
void TIM3_IRQHandler(void);
void TIM4_IRQHandler(void);
void TIM5_IRQHandler(void);

#endif /* TIM_INTERFACE_H */