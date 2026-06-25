/**
 ******************************************************************************
 * @file    IWDG_interface.h
 * @brief   Independent Watchdog (IWDG) Driver Interface for STM32F401CC
 * @author  Abdulrahman
 * @date    January 2026
 ******************************************************************************
 * @attention
 *
 * This file provides the public interface for the IWDG driver.
 * It defines all user-accessible functions, types, and configurations
 * for initializing and managing the Independent Watchdog peripheral.
 *
 * Key Features:
 * - Simple initialization with timeout calculation
 * - Periodic refresh (kick) functionality
 * - Timeout configuration in milliseconds
 * - Safe startup sequence handling
 * - Status checking capabilities
 *
 ******************************************************************************
 */

#ifndef IWDG_INTERFACE_H
#define IWDG_INTERFACE_H

#include "STD_TYPES.h"

/*===========================================================================*/
/*                          CONFIGURATION TYPES                               */
/*===========================================================================*/

/**
 * @brief IWDG Prescaler Options
 * 
 * Divides the 32 kHz LSI clock to generate different timeout ranges.
 * Lower prescaler = shorter max timeout, higher precision
 * Higher prescaler = longer max timeout, lower precision
 */
typedef enum {
    IWDG_PRESCALER_DIV_4    = 0x0U,    /**< Divide by 4   → 8 kHz   (0.125ms - 512ms)   */
    IWDG_PRESCALER_DIV_8    = 0x1U,    /**< Divide by 8   → 4 kHz   (0.25ms - 1024ms)   */
    IWDG_PRESCALER_DIV_16   = 0x2U,    /**< Divide by 16  → 2 kHz   (0.5ms - 2048ms)    */
    IWDG_PRESCALER_DIV_32   = 0x3U,    /**< Divide by 32  → 1 kHz   (1ms - 4096ms)      */
    IWDG_PRESCALER_DIV_64   = 0x4U,    /**< Divide by 64  → 500 Hz  (2ms - 8192ms)      */
    IWDG_PRESCALER_DIV_128  = 0x5U,    /**< Divide by 128 → 250 Hz  (4ms - 16384ms)     */
    IWDG_PRESCALER_DIV_256  = 0x6U     /**< Divide by 256 → 125 Hz  (8ms - 32768ms)     */
} IWDG_Prescaler_t;


/**
 * @brief IWDG Configuration Structure
 * 
 * Contains all parameters needed to initialize the IWDG.
 * User can either:
 *   1. Specify timeout_ms (driver calculates prescaler and reload)
 *   2. Manually specify prescaler and reload_value
 */
typedef struct {
    uint32_t timeout_ms;               /**< Desired timeout in milliseconds (1-32768 ms)
                                            Set to 0 if using manual prescaler/reload */
    
    IWDG_Prescaler_t prescaler;        /**< Prescaler divider (if manual configuration) */
    
    uint16_t reload_value;             /**< Reload value 1-4095 (if manual configuration)
                                            0 is INVALID — causes immediate MCU reset on
                                            next LSI tick. Valid range: IWDG_RLR_MIN(1)
                                            to IWDG_RLR_MAX(4095). */
    
    uint32_t lsi_frequency;            /**< LSI clock frequency in Hz (default: 32000)
                                            Set to 0 to use default 32kHz
                                            Useful if LSI frequency has been measured */
} IWDG_Config_t;


/**
 * @brief IWDG Status Return Codes
 * 
 * All IWDG functions return these status codes to indicate success or failure.
 */
typedef enum {
    IWDG_OK             = 0x00U,       /**< Operation completed successfully           */
    IWDG_ERROR          = 0x01U,       /**< Generic error occurred                     */
    IWDG_TIMEOUT        = 0x02U,       /**< Timeout waiting for hardware SR flags      */
    IWDG_INVALID_PARAM  = 0x03U,       /**< Invalid parameter provided                 */
    IWDG_NOT_STARTED    = 0x04U,       /**< IWDG not initialized — call IWDG_Init first*/
    IWDG_THREAD_HANG    = 0x05U        /**< Supervisor: one or more threads did not
                                            set their alive flag within the timeout.
                                            IWDG feed withheld — MCU will reset.       */
} IWDG_Status_t;


/**
 * @brief IWDG State
 * 
 * Tracks the current state of the IWDG peripheral.
 */
typedef enum {
    IWDG_STATE_RESET    = 0x00U,       /**< IWDG not initialized */
    IWDG_STATE_READY    = 0x01U,       /**< IWDG configured but not started */
    IWDG_STATE_RUNNING  = 0x02U        /**< IWDG running (cannot be stopped) */
} IWDG_State_t;


/*===========================================================================*/
/*                          PUBLIC FUNCTIONS                                  */
/*===========================================================================*/

/**
 * @brief Initialize the IWDG with automatic timeout calculation
 * 
 * This function configures the IWDG based on the desired timeout in milliseconds.
 * It automatically calculates the optimal prescaler and reload values.
 * 
 * @param timeout_ms: Desired timeout in milliseconds (range: 1 to 32768 ms)
 * 
 * @return IWDG_Status_t:
 *         - IWDG_OK: Initialization successful
 *         - IWDG_INVALID_PARAM: timeout_ms out of range
 *         - IWDG_TIMEOUT: Hardware timeout during configuration
 * 
 * @note This function does NOT start the watchdog. Call IWDG_Start() to begin.
 * @note Once started, the watchdog CANNOT be stopped except by a power cycle.
 *       NVIC_SystemReset() does NOT stop the IWDG — it survives software reset.
 * @note LSI frequency tolerance is 17–47 kHz. A 1000 ms nominal timeout can
 *       fire as fast as ~681 ms (at 47 kHz LSI). Design feed intervals against
 *       the worst-case fast end, not the nominal value.
 * @note For the FreeRTOS application: call IWDG_Start() ONLY after all three
 *       FreeRTOS threads are created and the scheduler has started.
 *       Do NOT call during the data-collection bare-metal phase.
 *
 * @usage
 *   IWDG_Init(1000U);   // Configure for 1 second timeout
 *   IWDG_Start();       // Start ONLY after vTaskStartScheduler() is running
 */
IWDG_Status_t IWDG_Init(uint32_t timeout_ms);


/**
 * @brief Initialize the IWDG with detailed configuration
 * 
 * This function provides full control over IWDG configuration.
 * Use this when you need precise control or have measured LSI frequency.
 * 
 * @param config: Pointer to IWDG_Config_t structure
 * 
 * @return IWDG_Status_t:
 *         - IWDG_OK: Initialization successful
 *         - IWDG_INVALID_PARAM: Invalid configuration parameters
 *         - IWDG_TIMEOUT: Hardware timeout during configuration
 * 
 * @note If config->timeout_ms is non-zero, prescaler and reload are calculated automatically.
 * @note If config->timeout_ms is zero, config->prescaler and config->reload_value are used.
 * @note If config->lsi_frequency is zero, default 32000 Hz is used.
 * 
 * @usage
 *   IWDG_Config_t cfg = {
 *       .timeout_ms = 5000,        // 5 second timeout
 *       .lsi_frequency = 32000     // 32 kHz LSI
 *   };
 *   IWDG_InitWithConfig(&cfg);
 *   IWDG_Start();
 */
IWDG_Status_t IWDG_InitWithConfig(const IWDG_Config_t* config);


/**
 * @brief Start the Independent Watchdog
 * 
 * Enables the IWDG counter. After this call, the watchdog CANNOT be stopped
 * except by system reset.
 * 
 * @return IWDG_Status_t:
 *         - IWDG_OK: Watchdog started successfully
 *         - IWDG_NOT_STARTED: IWDG not initialized (call IWDG_Init first)
 * 
 * @warning Once started, you MUST call IWDG_Refresh() periodically or the
 *          microcontroller will reset when the timeout expires!
 * 
 * @note Call IWDG_Init() or IWDG_InitWithConfig() before calling this function.
 * 
 * @usage
 *   IWDG_Init(2000);
 *   IWDG_Start();  // Watchdog is now running!
 */
IWDG_Status_t IWDG_Start(void);


/**
 * @brief Refresh (kick) the watchdog counter
 * 
 * Reloads the watchdog counter with the configured reload value.
 * This prevents the watchdog from timing out and resetting the system.
 * 
 * @return IWDG_Status_t:
 *         - IWDG_OK: Counter refreshed successfully
 * 
 * @note Must be called periodically before timeout expires.
 * @note Can be called even if watchdog is not started (no effect before start).
 * @note Refresh rate should be significantly faster than timeout period.
 *       Example: For 2s timeout, refresh every 1s or faster.
 * 
 * @usage
 *   // ✅ CORRECT — bare-metal main loop:
 *   while(1U) {
 *       Process_Tasks();
 *       IWDG_Refresh();
 *   }
 *
 *   // ❌ WRONG — do NOT call from a timer ISR:
 *   // void TIM2_IRQHandler(void) { IWDG_Refresh(); }
 *   // ISR fires even when main logic is deadlocked → watchdog never triggers.
 *
 *   // ❌ WRONG for FreeRTOS — do NOT call IWDG_Refresh() directly from threads.
 *   // Use IWDG_Thread_SetAlive() + IWDG_SupervisorFeed() pattern instead.
 */
IWDG_Status_t IWDG_Refresh(void);


/**
 * @brief Get current IWDG state
 * 
 * Returns the current state of the IWDG peripheral.
 * 
 * @return IWDG_State_t:
 *         - IWDG_STATE_RESET: Not initialized
 *         - IWDG_STATE_READY: Configured but not started
 *         - IWDG_STATE_RUNNING: Running (counting down)
 * 
 * @usage
 *   if(IWDG_GetState() == IWDG_STATE_RUNNING) {
 *       // Watchdog is active
 *   }
 */
IWDG_State_t IWDG_GetState(void);


/**
 * @brief Check if IWDG is currently running
 * 
 * @return uint8_t:
 *         - 1: IWDG is running
 *         - 0: IWDG is not running
 * 
 * @usage
 *   if(IWDG_IsRunning()) {
 *       IWDG_Refresh();
 *   }
 */
uint8_t IWDG_IsRunning(void);


/**
 * @brief Calculate actual timeout based on configuration
 * 
 * Returns the actual timeout in milliseconds based on current prescaler
 * and reload values. Useful for verification after initialization.
 * 
 * @param lsi_freq: LSI frequency in Hz (use 32000 if unknown)
 * 
 * @return uint32_t: Actual timeout in milliseconds
 * 
 * @note This reads the current hardware configuration.
 * @note Returns 0 if IWDG is not initialized.
 * 
 * @usage
 *   IWDG_Init(2000);
 *   uint32_t actual = IWDG_GetActualTimeout(32000);
 *   // actual should be close to 2000 ms
 */
uint32_t IWDG_GetActualTimeout(uint32_t lsi_freq);


/**
 * @brief Get configured prescaler value
 * 
 * @return IWDG_Prescaler_t: Current prescaler configuration
 * 
 * @usage
 *   IWDG_Prescaler_t prescaler = IWDG_GetPrescaler();
 */
IWDG_Prescaler_t IWDG_GetPrescaler(void);


/**
 * @brief Get configured reload value
 * 
 * @return uint16_t: Current reload value (1–4095, or 0 if not yet configured)
 * 
 * @usage
 *   uint16_t reload = IWDG_GetReloadValue();
 */
uint16_t IWDG_GetReloadValue(void);


/*===========================================================================*/
/*                    FREERTOS THREAD-AWARE WATCHDOG                          */
/*===========================================================================*/

/**
 * @brief Per-thread alive flags — written by each thread, read by supervisor.
 *
 * Each flag must be set by its owning thread exactly once per supervisor cycle
 * (i.e., at least once per IWDG timeout period). The supervisor clears all
 * flags atomically after a successful feed.
 *
 * Ownership:
 *   IWDG_Thread1_Alive — set by Thread 1 after each I2C DMA sensor read
 *   IWDG_Thread2_Alive — set by Thread 2 after each inference completes
 *   IWDG_Thread3_Alive — set by Thread 3 after each UART frame is transmitted
 *
 * Thread-safety:
 *   Each flag has exactly ONE writer (its thread) and ONE reader (supervisor).
 *   No mutex is required — SPSC discipline with volatile guarantees visibility.
 *   volatile prevents the compiler from caching the value in a register across
 *   the scheduler preemption boundary.
 *
 * MISRA Rule 8.4: extern declarations must match their definitions in IWDG.c.
 *
 * ⚠️  DO NOT set these flags from an ISR or timer callback.
 *     They must only be set from within the thread's main execution body
 *     to prove that the thread's LOGIC executed, not just that an interrupt fired.
 */
extern volatile uint8_t IWDG_Thread1_Alive;
extern volatile uint8_t IWDG_Thread2_Alive;
extern volatile uint8_t IWDG_Thread3_Alive;
extern volatile uint8_t IWDG_Thread4_Alive;


/**
 * @brief Mark the calling thread as alive for this supervisor cycle.
 *
 * Call this once per main loop iteration from within the thread body,
 * after the thread's primary work is complete.
 *
 * @param flag  Pointer to the thread's own alive flag.
 *              Pass &IWDG_Thread1_Alive, &IWDG_Thread2_Alive, or
 *              &IWDG_Thread3_Alive — never pass a local variable.
 *
 * Usage (Thread 1 example):
 *   // After DMA read and ring buffer push are complete:
 *   IWDG_Thread_SetAlive(&IWDG_Thread1_Alive);
 *
 * MISRA Rule 2.7: parameter 'flag' is used — no unused-parameter violation.
 * MISRA Rule 11.5: volatile uint8_t* is an appropriate pointer type here;
 *                  no cast to/from void* occurs.
 */
void IWDG_Thread_SetAlive(volatile uint8_t *flag);


/**
 * @brief Supervisor feed — call from FreeRTOS idle hook or lowest-priority task.
 *
 * Feeds the IWDG ONLY when ALL three thread alive flags are set.
 * If any flag is clear, the corresponding thread has not executed since the
 * last feed — the feed is withheld and the IWDG will reset the MCU.
 *
 * On successful feed:
 *   - All three flags are cleared to 0U atomically before the feed.
 *   - IWDG->KR is written with IWDG_KEY_RELOAD.
 *   - Returns IWDG_OK.
 *
 * On failed feed (thread hang detected):
 *   - Flags are NOT cleared (preserves state for post-reset diagnosis).
 *   - IWDG is NOT fed.
 *   - Returns IWDG_THREAD_HANG.
 *   - MCU will reset within one IWDG timeout period.
 *
 * @return IWDG_Status_t:
 *         IWDG_OK          — all threads alive, watchdog fed
 *         IWDG_THREAD_HANG — at least one thread did not signal liveness
 *
 * ⚠️  CALL THIS FROM THE IDLE HOOK OR A DEDICATED SUPERVISOR TASK ONLY.
 *     Never call from a timer ISR — the ISR fires even when threads are
 *     deadlocked, which defeats the entire purpose of the watchdog.
 *
 * ⚠️  CALL INTERVAL: must be shorter than IWDG worst-case timeout.
 *     For IWDG_CONFIG_1S (PR=/64, RL=500): worst-case timeout ~681 ms at
 *     LSI_MAX (47 kHz). Call supervisor at least every 500 ms.
 *
 * Placement in FreeRTOS:
 *   Option A (recommended) — vApplicationIdleHook():
 *     void vApplicationIdleHook(void) {
 *         IWDG_SupervisorFeed();
 *     }
 *
 *   Option B — dedicated lowest-priority task (priority 0):
 *     void supervisor_task(void *arg) {
 *         (void)arg;
 *         for(;;) {
 *             vTaskDelay(pdMS_TO_TICKS(500U));
 *             IWDG_SupervisorFeed();
 *         }
 *     }
 */
IWDG_Status_t IWDG_SupervisorFeed(void);


/*===========================================================================*/
/*                          UTILITY FUNCTIONS                                 */
/*===========================================================================*/

/**
 * @brief Calculate optimal prescaler and reload for desired timeout
 * 
 * This is a helper function that calculates the best prescaler and reload
 * values to achieve a desired timeout. Useful for pre-calculation or validation.
 * 
 * @param timeout_ms: Desired timeout in milliseconds
 * @param lsi_freq: LSI frequency in Hz (use 32000 if unknown)
 * @param prescaler: Pointer to store calculated prescaler
 * @param reload: Pointer to store calculated reload value
 * 
 * @return IWDG_Status_t:
 *         - IWDG_OK: Calculation successful
 *         - IWDG_INVALID_PARAM: Timeout out of achievable range or NULL pointers
 * 
 * @note This function does not modify hardware, only performs calculation.
 * 
 * @usage
 *   IWDG_Prescaler_t prescaler;
 *   uint16_t reload;
 *   if(IWDG_CalculateTimeout(5000, 32000, &prescaler, &reload) == IWDG_OK) {
 *       // Use calculated values
 *   }
 */
IWDG_Status_t IWDG_CalculateTimeout(uint32_t timeout_ms, 
                                     uint32_t lsi_freq,
                                     IWDG_Prescaler_t* prescaler,
                                     uint16_t* reload);


/**
 * @brief Verify timeout is achievable
 * 
 * Checks if a desired timeout can be achieved with the IWDG hardware.
 * 
 * @param timeout_ms: Desired timeout in milliseconds
 * @param lsi_freq: LSI frequency in Hz (use 32000 if unknown)
 * 
 * @return uint8_t:
 *         - 1: Timeout is achievable
 *         - 0: Timeout is out of range
 * 
 * @usage
 *   if(IWDG_IsTimeoutValid(50000, 32000)) {
 *       // 50 second timeout is possible
 *   }
 */
uint8_t IWDG_IsTimeoutValid(uint32_t timeout_ms, uint32_t lsi_freq);


/*===========================================================================*/
/*                          PRE-DEFINED CONFIGURATIONS                        */
/*===========================================================================*/

/**
 * @brief Quick initialization macros for common timeout values
 * 
 * These macros provide convenient one-line initialization for standard timeouts.
 * All values assume typical 32 kHz LSI frequency.
 */

/**
 * @brief Initialize IWDG for 1 second timeout
 * @usage IWDG_INIT_1_SECOND();
 */
#define IWDG_INIT_1_SECOND()        IWDG_Init(1000U)

/**
 * @brief Initialize IWDG for 2 second timeout
 * @usage IWDG_INIT_2_SECONDS();
 */
#define IWDG_INIT_2_SECONDS()       IWDG_Init(2000U)

/**
 * @brief Initialize IWDG for 5 second timeout
 * @usage IWDG_INIT_5_SECONDS();
 */
#define IWDG_INIT_5_SECONDS()       IWDG_Init(5000U)

/**
 * @brief Initialize IWDG for 10 second timeout
 * @usage IWDG_INIT_10_SECONDS();
 */
#define IWDG_INIT_10_SECONDS()      IWDG_Init(10000U)


/*===========================================================================*/
/*                          CONSTANTS                                         */
/*===========================================================================*/

/* Timeout Limits (at typical 32 kHz LSI) */
#define IWDG_TIMEOUT_MIN_MS         1U          /**< Minimum achievable timeout */
#define IWDG_TIMEOUT_MAX_MS         32768U      /**< Maximum achievable timeout */

/* Default LSI Frequency */
#define IWDG_DEFAULT_LSI_FREQ       32000U      /**< Default LSI frequency in Hz */

/* LSI frequency tolerance range — see IWDG_REGS.h: IWDG_LSI_FREQ_MIN/MAX (17–47 kHz) */


/*===========================================================================*/
/*                          USAGE EXAMPLES                                    */
/*===========================================================================*/

/**
 * @example Basic Usage - Simple 2 Second Watchdog
 * 
 * @code
 * #include "IWDG_interface.h"
 * 
 * int main(void) {
 *     
 *    
 *     
 *     
 *     
 *     
 *     // Start the watchdog
 *     IWDG_Start();
 *     
 *     
 *         
 *         
 *         
 *         
 *         
 *         
 *         
 *     
 * }
 * @endcode
 */

/**
 * @example Advanced Usage - Manual Configuration with Measured LSI
 * 
 * @code
 * #include "IWDG_interface.h"
 * 
 * int main(void) {
 *     // Measure LSI frequency (implementation not shown)
 *     uint32_t measured_lsi = Measure_LSI_Frequency();
 *     
 *     // Configure with measured LSI for better accuracy
*         .timeout_ms    = 5000U,          // 5 second timeout                  
 *         .lsi_frequency = measured_lsi,   // Use measured value                
 *         .prescaler     = IWDG_PRESCALER_DIV_4,  // Placeholder — auto-calculated 
 *         .reload_value  = 1U              // Placeholder — auto-calculated     
 *     };
 *     
 *     IWDG_InitWithConfig(&config);
 *     
 *     // Verify actual timeout
 *     uint32_t actual = IWDG_GetActualTimeout(measured_lsi);
 *     printf("Actual timeout: %lu ms\n", actual);
 *     
 *     IWDG_Start();
 *     
 *     while(1) {
 *         // Task execution
 *         Execute_Critical_Task();
 *         IWDG_Refresh();
 *     }
 * }
 * @endcode
 */

/**
 * @example FreeRTOS Usage - Thread-Aware Supervisor Pattern
 *
 * @code
 * #include "IWDG_INTERFACE.h"
 *
 * // In main() — after creating all three threads, before vTaskStartScheduler():
 * int main(void) {
 *     IWDG_Init(1000U);    // 1 second timeout, ~681 ms worst-case at 47 kHz LSI
 *     // ... create Thread 1, Thread 2, Thread 3 ...
 *     IWDG_Start();        // Start AFTER threads are created
 *     vTaskStartScheduler();
 * }
 *
 * // In Thread 1 — after each sensor read:
 * void sensor_task(void *arg) {
 *     (void)arg;
 *     for(;;) {
 *         xSemaphoreTake(timer_sem, portMAX_DELAY);
 *         MPU6050_Read();
 *         RingBuffer_Push();
 *         IWDG_Thread_SetAlive(&IWDG_Thread1_Alive);  // signal liveness
 *     }
 * }
 *
 * // Supervisor — in vApplicationIdleHook() or lowest-priority task:
 * void vApplicationIdleHook(void) {
 *     (void)IWDG_SupervisorFeed();
 *     // Returns IWDG_THREAD_HANG if any thread is stuck -> MCU resets
 * }
 * @endcode
 */


#endif /* IWDG_INTERFACE_H */

/***************************** END OF FILE ************************************/
