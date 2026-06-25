/**
 ******************************************************************************
 * @file    IWDG_program.c
 * @brief   Independent Watchdog (IWDG) Driver Implementation for STM32F401CC
 * @author  Abdulrahman
 * @date    January 2026
 ******************************************************************************
 */

#include "IWDG_INTERFACE.h"
#include "IWDG_REGS.h"
#include "STD_TYPES.h"

/*===========================================================================*/
/*                          PRIVATE VARIABLES                                 */
/*===========================================================================*/

/**
 * @brief Internal driver state tracking
 *
 * These static variables maintain the state of the IWDG driver across
 * function calls. They are private to this file and cannot be accessed
 * directly by users.
 */
static IWDG_State_t iwdg_state = IWDG_STATE_RESET;
static IWDG_Prescaler_t current_prescaler = IWDG_PRESCALER_DIV_4;
static uint16_t current_reload = 0U;


/*===========================================================================*/
/*                   FREERTOS THREAD-ALIVE FLAG DEFINITIONS                   */
/*===========================================================================*/

/**
 * @brief Per-thread alive flags — definitions matching extern declarations
 *        in IWDG_INTERFACE.h.
 *
 * Each flag is written by exactly ONE thread and read by the supervisor.
 * volatile ensures visibility across FreeRTOS context switches.
 *
 * MISRA Rule 8.4: these definitions match the extern declarations in
 *                 IWDG_INTERFACE.h exactly (volatile uint8_t).
 */
volatile uint8_t IWDG_Thread1_Alive = 0U;
volatile uint8_t IWDG_Thread2_Alive = 0U;
volatile uint8_t IWDG_Thread3_Alive = 0U;
volatile uint8_t IWDG_Thread4_Alive = 0U;


/*===========================================================================*/
/*                          PRIVATE HELPER FUNCTIONS                          */
/*===========================================================================*/

/**
 * @brief Wait for IWDG status register flags to clear
 *
 * This helper function polls the IWDG status register until both PVU and RVU
 * flags are cleared, indicating that the hardware has finished applying
 * configuration changes.
 *
 * @return IWDG_Status_t:
 *         - IWDG_OK: Flags cleared successfully
 *         - IWDG_TIMEOUT: Timeout waiting for hardware
 *
 * @note Uses IWDG_SR_POLL_TIMEOUT to guarantee bounded iteration count
 *       (MISRA Rule 14.4: loop always terminates).
 */
static IWDG_Status_t IWDG_WaitForUpdate(void)
{
    IWDG_Status_t status = IWDG_OK;
    uint32_t timeout_counter = 0U;

    /* Wait while either PVU (bit 0) or RVU (bit 1) is set.
     * These bits indicate hardware is still processing previous writes.
     * MISRA Rule 14.4: controlling expression is essentially boolean. */
    while ((IWDG->SR.ALL & IWDG_SR_UPDATE_MASK) != 0U)
    {
        timeout_counter++;

        /* Prevent infinite loop — bounded by IWDG_SR_POLL_TIMEOUT */
        if (timeout_counter > IWDG_SR_POLL_TIMEOUT)
        {
            status = IWDG_TIMEOUT;
            break;
        }
    }

    return status;
}


/**
 * @brief Get actual prescaler division value from enum
 *
 * Converts the prescaler enum to the actual division value.
 * Example: IWDG_PRESCALER_DIV_64 → 64
 *
 * @param prescaler: Prescaler enum value
 * @return uint32_t: Actual division value (4, 8, 16, 32, 64, 128, or 256)
 */
static uint32_t IWDG_GetPrescalerValue(IWDG_Prescaler_t prescaler)
{
    static const uint32_t prescaler_values[8U] = {4U, 8U, 16U, 32U, 64U, 128U, 256U, 256U};
    uint32_t result;

    /* Ensure index is within bounds */
    if (prescaler > IWDG_PRESCALER_DIV_256)
    {
        result = 256U;  /* Default to maximum */
    }
    else
    {
        result = prescaler_values[(uint32_t)prescaler];
    }

    return result;
}


/**
 * @brief Configure IWDG hardware registers
 *
 * This private helper function performs the actual hardware configuration.
 * It is used by both IWDG_Init() and IWDG_InitWithConfig() to avoid
 * code duplication.
 *
 * Sequence (per RM0368 §20.3.2):
 *   1. Write 0x5555 to KR  → unlock PR and RLR
 *   2. Wait for PVU/RVU to clear
 *   3. Write prescaler to PR
 *   4. Write reload value to RLR
 *   5. Wait for PVU/RVU to clear (hardware applies values)
 *   6. Write 0xAAAA to KR  → reload counter with new RLR value
 *
 * @param prescaler: Prescaler value to write
 * @param reload: Reload value to write
 * @return IWDG_Status_t: Status of configuration
 */
static IWDG_Status_t IWDG_ConfigureHardware(IWDG_Prescaler_t prescaler, uint16_t reload)
{
    IWDG_Status_t status = IWDG_OK;

    IWDG_ENABLE_WRITE();
    
    /* Only wait if IWDG is already running — otherwise PVU/RVU are stale */
    if (iwdg_state == IWDG_STATE_RUNNING)
    {
        status = IWDG_WaitForUpdate();
        if (status != IWDG_OK) return status;
    }
    
    IWDG_SET_PRESCALER((uint32_t)prescaler);
    IWDG_SET_RELOAD((uint32_t)reload);
    
    if (iwdg_state == IWDG_STATE_RUNNING)
    {
        status = IWDG_WaitForUpdate();
        if (status != IWDG_OK) return status;
    }
    
    IWDG_RELOAD();
    
    current_prescaler = prescaler;
    current_reload    = reload;
    if (iwdg_state == IWDG_STATE_RESET) {
        iwdg_state = IWDG_STATE_READY;
    }
    
    return status;
}


/*===========================================================================*/
/*                          PUBLIC FUNCTION IMPLEMENTATIONS                   */
/*===========================================================================*/

/**
 * @brief Calculate optimal prescaler and reload for desired timeout
 *
 * Algorithm:
 * 1. Try each prescaler from smallest (4) to largest (256)
 * 2. For each prescaler, calculate required reload value
 * 3. Check if reload fits in valid range [IWDG_RLR_MIN, IWDG_RLR_MAX]
 * 4. Select first configuration that gives a valid reload value
 *    (smallest prescaler → best resolution)
 *
 * Formula: reload = (timeout_ms × lsi_freq) / (prescaler_div × 1000)
 */
IWDG_Status_t IWDG_CalculateTimeout(uint32_t timeout_ms,
                                     uint32_t lsi_freq,
                                     IWDG_Prescaler_t* prescaler,
                                     uint16_t* reload)
{
    IWDG_Status_t status = IWDG_INVALID_PARAM;
    uint32_t local_lsi_freq;
    uint8_t pr;

    /* Step 1: Validate input parameters */
    if ((prescaler == ((void *)0)) || (reload == ((void *)0)))
    {
        /* status already IWDG_INVALID_PARAM */
    }
    else if ((timeout_ms < IWDG_TIMEOUT_MIN_MS) || (timeout_ms > IWDG_TIMEOUT_MAX_MS))
    {
        /* status already IWDG_INVALID_PARAM */
    }
    else
    {
        /* Use default LSI frequency if not specified */
        if (lsi_freq == 0U)
        {
            local_lsi_freq = IWDG_DEFAULT_LSI_FREQ;
        }
        else
        {
            local_lsi_freq = lsi_freq;
        }

        /* Step 2: Try each prescaler to find valid configuration.
         * Start with smallest prescaler for better precision. */
        for (pr = (uint8_t)IWDG_PRESCALER_DIV_4; pr <= (uint8_t)IWDG_PRESCALER_DIV_256; pr++)
        {
            /* Get actual prescaler division value (4, 8, 16, …) */
            uint32_t prescaler_div = IWDG_GetPrescalerValue((IWDG_Prescaler_t)pr);

            /* Calculate required reload value.
             * Use 64-bit arithmetic to prevent overflow.
             * Formula: reload = (timeout_ms × lsi_freq) / (prescaler_div × 1000) */
            uint64_t reload_calc = ((uint64_t)timeout_ms * (uint64_t)local_lsi_freq)
                                   / ((uint64_t)prescaler_div * 1000ULL);

            /* Check if reload value fits in valid range [1, 4095] */
            if ((reload_calc >= 1ULL) &&
               (reload_calc <= (uint64_t)IWDG_RLR_MAX))
            {
                /* Found a valid configuration */
                *prescaler = (IWDG_Prescaler_t)pr;
                *reload = (uint16_t)reload_calc;
                status = IWDG_OK;
                break;
            }
        }
        /* If loop completes without break, status remains IWDG_INVALID_PARAM
         * (timeout is out of achievable range). */
    }

    return status;
}


/**
 * @brief Verify timeout is achievable
 *
 * @param timeout_ms: Desired timeout in milliseconds
 * @param lsi_freq: LSI frequency in Hz (use 32000 if unknown)
 *
 * @return uint8_t:
 *         - 1U: Timeout is achievable
 *         - 0U: Timeout is out of range
 */
uint8_t IWDG_IsTimeoutValid(uint32_t timeout_ms, uint32_t lsi_freq)
{
    IWDG_Prescaler_t dummy_prescaler;
    uint16_t dummy_reload;
    uint8_t result;

    IWDG_Status_t status = IWDG_CalculateTimeout(timeout_ms,
                                                   lsi_freq,
                                                   &dummy_prescaler,
                                                   &dummy_reload);

    /* MISRA Rule 14.4: explicit boolean conversion */
    result = (status == IWDG_OK) ? 1U : 0U;

    return result;
}


/**
 * @brief Initialize the IWDG with automatic timeout calculation
 *
 * Creates a config structure in automatic mode and delegates to
 * IWDG_InitWithConfig().
 */
IWDG_Status_t IWDG_Init(uint32_t timeout_ms)
{
    IWDG_Config_t config;

    config.timeout_ms    = timeout_ms;
    config.prescaler     = IWDG_PRESCALER_DIV_4;   /* Will be calculated */
    config.reload_value  = 1U;                       /* Will be calculated */
    config.lsi_frequency = 0U;                      /* Use default 32 kHz */

    return IWDG_InitWithConfig(&config);
}


/**
 * @brief Initialize the IWDG with detailed configuration
 *
 * Two configuration modes:
 *
 * Mode 1 (Automatic): config->timeout_ms != 0
 *   - Calculates prescaler and reload from timeout_ms
 *   - Optionally uses custom LSI frequency
 *
 * Mode 2 (Manual): config->timeout_ms == 0
 *   - Uses user-provided prescaler and reload values directly
 *   - No calculation performed
 */
IWDG_Status_t IWDG_InitWithConfig(const IWDG_Config_t* config)
{
    IWDG_Status_t status;
    IWDG_Prescaler_t final_prescaler = IWDG_PRESCALER_DIV_4;
    uint16_t final_reload = 0U;
    uint32_t lsi_freq;

    /* Step A: Validate config pointer */
    if (config == ((void *)0))
    {
        status = IWDG_INVALID_PARAM;
    }
    else
    {
        /* Step B: Determine LSI frequency to use */
        if (config->lsi_frequency == 0U)
        {
            lsi_freq = IWDG_DEFAULT_LSI_FREQ;
        }
        else
        {
            lsi_freq = config->lsi_frequency;
        }

        /* Step C: Determine configuration mode */
        if (config->timeout_ms != 0U)
        {
            /* Mode 1: AUTOMATIC — calculate from timeout_ms */
            if ((config->timeout_ms < IWDG_TIMEOUT_MIN_MS) ||
                (config->timeout_ms > IWDG_TIMEOUT_MAX_MS))
            {
                status = IWDG_INVALID_PARAM;
            }
            else
            {
                status = IWDG_CalculateTimeout(config->timeout_ms,
                                                lsi_freq,
                                                &final_prescaler,
                                                &final_reload);
            }
        }
        else
        {
            /* Mode 2: MANUAL — use provided prescaler and reload */
            if (config->prescaler > IWDG_PRESCALER_DIV_256)
            {
                status = IWDG_INVALID_PARAM;
            }
            else if ((config->reload_value == 0U) ||
                     ((uint32_t)config->reload_value > IWDG_RLR_MAX))
            {
                status = IWDG_INVALID_PARAM;
            }
            else
            {
                final_prescaler = config->prescaler;
                final_reload = config->reload_value;
                status = IWDG_OK;
            }
        }

        /* Step D: Configure hardware if parameters are valid */
        if (status == IWDG_OK)
        {
            status = IWDG_ConfigureHardware(final_prescaler, final_reload);
        }
    }

    return status;
}


/**
 * @brief Start the Independent Watchdog
 *
 * Enables the IWDG counter. After this call, the watchdog CANNOT be stopped
 * except by a power cycle — it survives NVIC_SystemReset().
 *
 * @return IWDG_Status_t:
 *         - IWDG_OK: Watchdog started successfully
 *         - IWDG_NOT_STARTED: IWDG not initialized (call IWDG_Init first)
 *
 * @warning IRREVERSIBLE — once started, you MUST feed periodically!
 */
IWDG_Status_t IWDG_Start(void)
{
    IWDG_Status_t status;

    /* Check if IWDG has been initialized */
    if (iwdg_state == IWDG_STATE_RESET)
    {
        status = IWDG_NOT_STARTED;
    }
    else
    {
        /* Write the start key — counter begins counting down.
         * WARNING: This cannot be undone except by power cycle! */
        IWDG_START();

        iwdg_state = IWDG_STATE_RUNNING;
        status = IWDG_OK;
    }

    return status;
}


/**
 * @brief Refresh (kick) the watchdog counter
 *
 * Reloads the watchdog counter with the configured reload value.
 * Safe to call even if watchdog is not started (no effect before start).
 *
 * @return IWDG_Status_t:
 *         - IWDG_OK: Counter refreshed successfully
 *
 * @note For FreeRTOS: do NOT call this directly from threads.
 *       Use IWDG_Thread_SetAlive() + IWDG_SupervisorFeed() instead.
 */
IWDG_Status_t IWDG_Refresh(void)
{
    /* Write the reload key — reloads counter with RLR value */
    IWDG_RELOAD();

    return IWDG_OK;
}


/**
 * @brief Get current IWDG state
 *
 * @return IWDG_State_t: Current state of the IWDG peripheral
 */
IWDG_State_t IWDG_GetState(void)
{
    return iwdg_state;
}


/**
 * @brief Check if IWDG is currently running
 *
 * @return uint8_t: 1U if running, 0U if not running
 */
uint8_t IWDG_IsRunning(void)
{
    uint8_t result;

    /* MISRA Rule 14.4: explicit boolean conversion */
    result = (iwdg_state == IWDG_STATE_RUNNING) ? 1U : 0U;

    return result;
}


/**
 * @brief Get configured prescaler value
 *
 * @return IWDG_Prescaler_t: Current prescaler configuration
 */
IWDG_Prescaler_t IWDG_GetPrescaler(void)
{
    return current_prescaler;
}


/**
 * @brief Get configured reload value
 *
 * @return uint16_t: Current reload value (1–4095, or 0 if not configured)
 */
uint16_t IWDG_GetReloadValue(void)
{
    return current_reload;
}


/**
 * @brief Calculate actual timeout based on current hardware configuration
 *
 * @param lsi_freq: LSI frequency in Hz (use 32000 if unknown, 0 for default)
 * @return uint32_t: Actual timeout in milliseconds (0 if not initialized)
 *
 * Formula: T_ms = (prescaler_div × reload × 1000) / lsi_freq
 */
uint32_t IWDG_GetActualTimeout(uint32_t lsi_freq)
{
    uint32_t result;
    uint32_t local_lsi_freq;

    /* Check if IWDG is initialized */
    if (iwdg_state == IWDG_STATE_RESET)
    {
        result = 0U;
    }
    else
    {
        /* Use default LSI frequency if not specified */
        if (lsi_freq == 0U)
        {
            local_lsi_freq = IWDG_DEFAULT_LSI_FREQ;
        }
        else
        {
            local_lsi_freq = lsi_freq;
        }

        /* Get prescaler division value */
        uint32_t prescaler_div = IWDG_GetPrescalerValue(current_prescaler);

        /* Calculate timeout:
         * T_ms = (prescaler_div × reload × 1000) / lsi_freq
         * Use uint64_t intermediate to prevent 32-bit overflow. */
        uint64_t timeout_calc = ((uint64_t)prescaler_div
                                 * (uint64_t)current_reload
                                 * 1000ULL)
                                / (uint64_t)local_lsi_freq;

        result = (uint32_t)timeout_calc;
    }

    return result;
}


/*===========================================================================*/
/*                    FREERTOS THREAD-AWARE WATCHDOG                          */
/*===========================================================================*/

/**
 * @brief Mark the calling thread as alive for this supervisor cycle.
 *
 * Sets the pointed-to flag to 1U. Call once per main loop iteration
 * from within the thread body, after the thread's primary work completes.
 *
 * @param flag  Pointer to the thread's own alive flag
 *              (&IWDG_Thread1_Alive, &IWDG_Thread2_Alive, or &IWDG_Thread3_Alive)
 *
 * MISRA Rule 2.7: parameter 'flag' is used.
 * MISRA Rule 11.5: no cast to/from void* occurs.
 */
void IWDG_Thread_SetAlive(volatile uint8_t *flag)
{
    if (flag != ((volatile uint8_t *)0))
    {
        *flag = 1U;
    }
}


/**
 * @brief Supervisor feed — call from idle hook or lowest-priority task.
 *
 * Feeds the IWDG ONLY when ALL three thread alive flags are set (1U).
 * If any flag is 0U, the corresponding thread has not executed since
 * the last successful feed — the feed is withheld and the IWDG will
 * reset the MCU within one timeout period.
 *
 * On success:
 *   - All three flags cleared to 0U before the feed
 *   - IWDG counter reloaded
 *   - Returns IWDG_OK
 *
 * On failure (thread hang):
 *   - Flags NOT cleared (preserves state for post-reset diagnosis)
 *   - IWDG NOT fed — MCU will reset
 *   - Returns IWDG_THREAD_HANG
 */
IWDG_Status_t IWDG_SupervisorFeed(void)
{
    IWDG_Status_t status;

    /* Check all three thread alive flags.
     * MISRA Rule 14.4: each comparison is essentially boolean. */
    if ((IWDG_Thread1_Alive != 0U) &&
        (IWDG_Thread2_Alive != 0U) &&
        (IWDG_Thread3_Alive != 0U) &&
        (IWDG_Thread4_Alive != 0U))
    {
        IWDG_Thread1_Alive = 0U;
        IWDG_Thread2_Alive = 0U;
        IWDG_Thread3_Alive = 0U;
        IWDG_Thread4_Alive = 0U;
        IWDG_RELOAD();
        status = IWDG_OK;
    }
    else
    {
        status = IWDG_THREAD_HANG;
    }

    return status;
}


/***************************** END OF FILE ************************************/
