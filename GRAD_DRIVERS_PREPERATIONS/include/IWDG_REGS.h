/**
 ******************************************************************************
 * @file    iwdg_regs.h
 * @brief   Independent Watchdog (IWDG) Register Definitions for STM32F401CC
 * @author  Abdulrahman
 * @date    January 2026
 ******************************************************************************
 * @attention
 *
 * This file provides low-level register definitions for the IWDG peripheral
 * in STM32F401CC microcontroller. All register addresses and bit definitions
 * are based on the STM32F401xB/xC reference manual (RM0368).
 *
 * Register access is provided through union structures allowing both:
 * - Direct 32-bit access via .ALL member
 * - Bit-field access via .BITS member
 *
 ******************************************************************************
 */

#ifndef IWDG_REGS_H
#define IWDG_REGS_H

#include "STD_TYPES.h"

/**
 * ============================================================================
 * IWDG_KR - Key Register
 * Address offset: 0x00
 * Reset value: 0x0000 0000 (write only)
 * ============================================================================
 * 
 * Key Values:
 * -----------
 * 0xCCCC - Start the watchdog (enables counter)
 * 0xAAAA - Reload counter with value from RLR (kick the watchdog)
 * 0x5555 - Enable write access to PR and RLR registers
 * 
 * Important: This is a write-only register
 * ============================================================================
 */
typedef union {
    volatile uint32_t ALL;
    struct {
        volatile uint32_t KEY        : 16; // Bits 15:0: Key Value
                                           //            Write only, reads as 0x0000
                                           //            0xCCCC: Start watchdog
                                           //            0xAAAA: Reload counter
                                           //            0x5555: Enable write access to PR and RLR
        
        volatile uint32_t RESERVED0  : 16; // Bits 31:16: Reserved, must be kept at reset value
    } BITS;
} IWDG_KR_t;


/**
 * ============================================================================
 * IWDG_PR - Prescaler Register
 * Address offset: 0x04
 * Reset value: 0x0000 0000
 * ============================================================================
 * 
 * Prescaler divides the 32 kHz LSI clock:
 * PR[2:0] = 000: divider /4    → 8 kHz
 * PR[2:0] = 001: divider /8    → 4 kHz
 * PR[2:0] = 010: divider /16   → 2 kHz
 * PR[2:0] = 011: divider /32   → 1 kHz
 * PR[2:0] = 100: divider /64   → 500 Hz
 * PR[2:0] = 101: divider /128  → 250 Hz
 * PR[2:0] = 110: divider /256  → 125 Hz
 * PR[2:0] = 111: divider /256  → 125 Hz (same as 110)
 * 
 * Must write 0x5555 to KR before writing to PR
 * Changes take effect after PVU flag in SR is cleared
 * ============================================================================
 */
typedef union {
    volatile uint32_t ALL;
    struct {
        volatile uint32_t PR         : 3;  // Bits 2:0: Prescaler Divider
                                           //           000: divider /4
                                           //           001: divider /8
                                           //           010: divider /16
                                           //           011: divider /32
                                           //           100: divider /64
                                           //           101: divider /128
                                           //           110: divider /256
                                           //           111: divider /256
        
        volatile uint32_t RESERVED0  : 29; // Bits 31:3: Reserved, must be kept at reset value
    } BITS;
} IWDG_PR_t;


/**
 * ============================================================================
 * IWDG_RLR - Reload Register
 * Address offset: 0x08
 * Reset value: 0x0000 0FFF (all 12 bits set)
 * ============================================================================
 * 
 * Contains the reload value for the watchdog counter.
 * Valid range: 0x000 to 0xFFF (0 to 4095)
 * 
 * Timeout calculation:
 * T_timeout = (Prescaler / LSI_freq) × Reload_value
 * Where: LSI_freq ≈ 32 kHz (typically 32 ±10% kHz)
 * 
 * Example: PR=4 (/64), RLR=1000
 * Timeout = (64 / 32000) × 1000 = 2 seconds
 * 
 * Must write 0x5555 to KR before writing to RLR
 * Changes take effect after RVU flag in SR is cleared
 * ============================================================================
 */
typedef union {
    volatile uint32_t ALL;
    struct {
        volatile uint32_t RL         : 12; // Bits 11:0: Watchdog Counter Reload Value
                                           //            Value loaded into the counter when:
                                           //            - 0xAAAA is written to KR (reload)
                                           //            - 0xCCCC is written to KR (start)
                                           //            Valid range: 0x000 to 0xFFF
        
        volatile uint32_t RESERVED0  : 20; // Bits 31:12: Reserved, must be kept at reset value
    } BITS;
} IWDG_RLR_t;


/**
 * ============================================================================
 * IWDG_SR - Status Register
 * Address offset: 0x0C
 * Reset value: 0x0000 0000 (read only)
 * ============================================================================
 * 
 * This register indicates if a write operation to PR or RLR is ongoing.
 * Software must wait for these flags to clear before the new values take effect.
 * 
 * Important: This is a read-only register
 * ============================================================================
 */
typedef union {
    volatile uint32_t ALL;
    struct {
        volatile uint32_t PVU        : 1;  // Bit 0:  Prescaler Value Update
                                           //         0: Prescaler value update operation completed
                                           //         1: Prescaler value update operation ongoing
                                           //         Set by hardware when PR is written
                                           //         Cleared by hardware when update complete
        
        volatile uint32_t RVU        : 1;  // Bit 1:  Reload Value Update
                                           //         0: Reload value update operation completed
                                           //         1: Reload value update operation ongoing
                                           //         Set by hardware when RLR is written
                                           //         Cleared by hardware when update complete
        
        volatile uint32_t RESERVED0  : 30; // Bits 31:2: Reserved, must be kept at reset value
    } BITS;
} IWDG_SR_t;


/**
 * ============================================================================
 * Complete IWDG Register Block Structure
 * ============================================================================
 * 
 * Base address for STM32F401:
 *   IWDG: 0x40003000
 * ============================================================================
 */
typedef struct {
    IWDG_KR_t   KR;     // 0x00: Key register (write only)
    IWDG_PR_t   PR;     // 0x04: Prescaler register
    IWDG_RLR_t  RLR;    // 0x08: Reload register
    IWDG_SR_t   SR;     // 0x0C: Status register (read only)
} IWDG_REGS_t;


/**
 * ============================================================================
 * IWDG Peripheral Base Address Definition
 * ============================================================================
 */
#define IWDG_BASEADDR           0x40003000UL
#define IWDG                    ((IWDG_REGS_t*)IWDG_BASEADDR)


/*===========================================================================*/
/*                          KEY VALUES                                        */
/*===========================================================================*/

/* Key Register Values
 * All written as (uint32_t) casts with U suffix per MISRA Rule 7.2 and 10.1.
 * KR is write-only — reads return 0x0000; never read KR to verify a write.
 */
#define IWDG_KEY_RELOAD         ((uint32_t)0xAAAAU)  /* Reload counter with RLR value    */
#define IWDG_KEY_ENABLE         ((uint32_t)0xCCCCU)  /* Start the watchdog (IRREVERSIBLE) */
#define IWDG_KEY_WR_ACCESS      ((uint32_t)0x5555U)  /* Unlock PR and RLR for writing    */


/*===========================================================================*/
/*                          PRESCALER VALUES                                  */
/*===========================================================================*/

/* Prescaler Divider Values (for PR register) */
#define IWDG_PRESCALER_4        0x0U        /* Divide by 4   → 8 kHz   */
#define IWDG_PRESCALER_8        0x1U        /* Divide by 8   → 4 kHz   */
#define IWDG_PRESCALER_16       0x2U        /* Divide by 16  → 2 kHz   */
#define IWDG_PRESCALER_32       0x3U        /* Divide by 32  → 1 kHz   */
#define IWDG_PRESCALER_64       0x4U        /* Divide by 64  → 500 Hz  */
#define IWDG_PRESCALER_128      0x5U        /* Divide by 128 → 250 Hz  */
#define IWDG_PRESCALER_256      0x6U        /* Divide by 256 → 125 Hz  */

/* Prescaler Actual Division Values */
#define IWDG_PRESCALER_DIV_4    4U
#define IWDG_PRESCALER_DIV_8    8U
#define IWDG_PRESCALER_DIV_16   16U
#define IWDG_PRESCALER_DIV_32   32U
#define IWDG_PRESCALER_DIV_64   64U
#define IWDG_PRESCALER_DIV_128  128U
#define IWDG_PRESCALER_DIV_256  256U


/*===========================================================================*/
/*                          RELOAD VALUES                                     */
/*===========================================================================*/

/* Reload Register Limits
 * RLR_MIN = 1U: a reload of 0 causes the counter to expire on the very
 * next LSI tick (~31 µs) — effectively disabling the MCU.  Never write 0.
 */
#define IWDG_RLR_MIN            ((uint32_t)0x001U)   /* Minimum valid reload value (1)    */
#define IWDG_RLR_MAX            ((uint32_t)0xFFFU)   /* Maximum reload value (4095, 12-bit)*/


/*===========================================================================*/
/*                          STATUS FLAGS                                      */
/*===========================================================================*/

/* Status Register Bit Positions */
#define IWDG_SR_PVU_POS         0U          /* Prescaler Value Update bit position */
#define IWDG_SR_RVU_POS         1U          /* Reload Value Update bit position    */

/* Status Register Bit Masks
 * Cast to uint32_t — SR is a 32-bit register; masking against a bare
 * hex literal produces a signed int intermediate (MISRA Rule 10.1 violation).
 */
#define IWDG_SR_PVU_MASK        ((uint32_t)0x0001U)  /* Prescaler update ongoing */
#define IWDG_SR_RVU_MASK        ((uint32_t)0x0002U)  /* Reload value update ongoing */
#define IWDG_SR_UPDATE_MASK     (IWDG_SR_PVU_MASK | IWDG_SR_RVU_MASK)


/*===========================================================================*/
/*                          CLOCK CONSTANTS                                   */
/*===========================================================================*/

/* LSI Clock Specifications
 *
 * RM0368 + constraint doc: LSI is NOT precision-trimmed.
 * Real-world range is 17 kHz to 47 kHz across temperature,
 * voltage, and silicon variation.
 *
 * Impact on timeout:
 *   A 500 ms nominal timeout can fire as fast as 340 ms (at 47 kHz)
 *   or as late as 941 ms (at 17 kHz).
 *
 * RULE: Always apply a 2x safety margin on your feed interval.
 *   If your longest code path is 200 ms, configure timeout >= 500 ms,
 *   and feed at least every 250 ms.
 */
#define IWDG_LSI_FREQ_TYP       32000U      /* Typical LSI frequency: 32 kHz          */
#define IWDG_LSI_FREQ_MIN       17000U      /* Minimum LSI frequency: 17 kHz (abs min) */
#define IWDG_LSI_FREQ_MAX       47000U      /* Maximum LSI frequency: 47 kHz (abs max) */


/*===========================================================================*/
/*                          TIMEOUT LIMITS                                    */
/*===========================================================================*/

/* Approximate Timeout Limits (in milliseconds) at NOMINAL 32 kHz LSI
 *
 * Min: PR=0 (/4), RL=1   → ~0.125 ms  — set to 1U as practical floor
 * Max: PR=6 (/256), RL=4095 → ~32768 ms
 *
 * ⚠️ At LSI_MAX (47 kHz) the actual timeout shrinks to ~68% of nominal.
 *    At LSI_MIN (17 kHz) the actual timeout stretches to ~188% of nominal.
 *    Design your feed interval to account for the fast end (LSI_MAX).
 */
#define IWDG_TIMEOUT_MIN_MS     1U          /* Minimum timeout in ms — practical floor   */
#define IWDG_TIMEOUT_MAX_MS     32768U      /* Maximum timeout in ms at nominal 32 kHz   */


/*===========================================================================*/
/*                          TIMEOUT VALUES                                    */
/*===========================================================================*/

/* Polling Timeout for Status Register Flags (in loop iterations)
 *
 * IWDG->SR flags (PVU, RVU) clear within 6 LSI cycles.
 * At LSI_MIN (17 kHz): 6 cycles = ~353 µs.
 * At 84 MHz CPU: 353 µs = ~29,600 NOP iterations.
 * 50000U provides ~1.7x margin over the worst-case LSI-min scenario.
 *
 * MISRA Rule 14.4: loop must have a provable upper bound — this constant
 * is that bound.  The loop in IWDG_WaitForUpdate() decrements a counter
 * against this value; the loop ALWAYS terminates.
 */
#define IWDG_SR_POLL_TIMEOUT    50000U      /* Max iterations waiting for PVU/RVU to clear */


/*===========================================================================*/
/*                          HELPER MACROS                                     */
/*===========================================================================*/

/**
 * @brief Check if prescaler update is ongoing
 * MISRA Rule 14.4: result of bitwise-AND is not implicitly boolean.
 * Explicit != 0U required.
 */
#define IWDG_IS_PVU_ONGOING()    ((IWDG->SR.ALL & IWDG_SR_PVU_MASK) != 0U)

/**
 * @brief Check if reload value update is ongoing
 */
#define IWDG_IS_RVU_ONGOING()    ((IWDG->SR.ALL & IWDG_SR_RVU_MASK) != 0U)

/**
 * @brief Check if any update is ongoing (use in while-loop condition)
 * MISRA Rule 14.4: controlling expression must be essentially boolean.
 */
#define IWDG_IS_UPDATE_ONGOING() ((IWDG->SR.ALL & IWDG_SR_UPDATE_MASK) != 0U)

/**
 * @brief Write key value to KR register
 * @param key: Use IWDG_KEY_RELOAD, IWDG_KEY_ENABLE, or IWDG_KEY_WR_ACCESS only.
 *             These are pre-cast to uint32_t — do not pass bare literals here.
 */
#define IWDG_WRITE_KEY(key)      (IWDG->KR.ALL = (key))

/**
 * @brief Enable write access to PR and RLR registers (must precede any PR/RLR write)
 */
#define IWDG_ENABLE_WRITE()      IWDG_WRITE_KEY(IWDG_KEY_WR_ACCESS)

/**
 * @brief Reload watchdog counter — equivalent to iwdg_refresh() at register level
 * @note  Do NOT call this from an ISR. Use the driver function iwdg_refresh() instead.
 */
#define IWDG_RELOAD()            IWDG_WRITE_KEY(IWDG_KEY_RELOAD)

/**
 * @brief Start the watchdog. IRREVERSIBLE — only call after full system init.
 */
#define IWDG_START()             IWDG_WRITE_KEY(IWDG_KEY_ENABLE)

/**
 * @brief Set prescaler value in PR register.
 * Caller MUST have called IWDG_ENABLE_WRITE() first.
 * @param prescaler: value 0U–6U (use IWDG_PRESCALER_x constants)
 */
#define IWDG_SET_PRESCALER(prescaler) \
    do { \
        IWDG->PR.ALL = ((uint32_t)(prescaler) & 0x7U); \
    } while(0)

/**
 * @brief Set reload value in RLR register.
 * Caller MUST have called IWDG_ENABLE_WRITE() first.
 * @param reload: value 1U–4095U (IWDG_RLR_MIN to IWDG_RLR_MAX)
 */
#define IWDG_SET_RELOAD(reload) \
    do { \
        IWDG->RLR.ALL = ((uint32_t)(reload) & (uint32_t)0xFFFU); \
    } while(0)

/**
 * @brief Read current prescaler register value (raw 0–6)
 */
#define IWDG_GET_PRESCALER()    (IWDG->PR.ALL  & 0x7U)

/**
 * @brief Read current reload register value (0–4095)
 */
#define IWDG_GET_RELOAD()       (IWDG->RLR.ALL & (uint32_t)0xFFFU)


/*===========================================================================*/
/*                          CALCULATION MACROS                                */
/*===========================================================================*/

/**
 * @brief Calculate nominal timeout in milliseconds given hardware configuration.
 * @param prescaler_div: Actual prescaler division value (4, 8, 16, 32, 64, 128, 256)
 * @param reload:        RLR reload value (1–4095)
 * @param lsi_freq:      LSI frequency in Hz — use IWDG_LSI_FREQ_TYP for nominal
 * @return Timeout in milliseconds (uint32_t)
 *
 * Formula: T_ms = (prescaler_div × reload × 1000) / lsi_freq
 * Use uint64_t intermediate to prevent 32-bit overflow on large values.
 */
#define IWDG_CALC_TIMEOUT_MS(prescaler_div, reload, lsi_freq) \
    ((uint32_t)(((uint64_t)(prescaler_div) * (uint64_t)(reload) * (uint64_t)1000U) \
                / (uint64_t)(lsi_freq)))

/**
 * @brief Calculate reload value needed for a desired timeout.
 * @param timeout_ms:    Desired timeout in milliseconds
 * @param prescaler_div: Actual prescaler division value (4, 8, 16, 32, 64, 128, 256)
 * @param lsi_freq:      LSI frequency in Hz — use IWDG_LSI_FREQ_TYP for nominal
 * @return Reload value (uint32_t) — caller must verify it fits in IWDG_RLR_MAX
 *
 * Formula: reload = (timeout_ms × lsi_freq) / (prescaler_div × 1000)
 */
#define IWDG_CALC_RELOAD(timeout_ms, prescaler_div, lsi_freq) \
    ((uint32_t)(((uint64_t)(timeout_ms) * (uint64_t)(lsi_freq)) \
                / ((uint64_t)(prescaler_div) * (uint64_t)1000U)))

/**
 * @brief Calculate the WORST-CASE (fastest) timeout in milliseconds.
 * Use LSI_FREQ_MAX (47000) to get the minimum possible timeout window.
 * Design your feed interval to always be shorter than this value.
 *
 * Example: IWDG_CALC_TIMEOUT_WORST_MS(64, 250, 47000) → ~340 ms
 *          → your feed interval must be < 340 ms, not < 500 ms nominal.
 */
#define IWDG_CALC_TIMEOUT_WORST_MS(prescaler_div, reload) \
    IWDG_CALC_TIMEOUT_MS((prescaler_div), (reload), IWDG_LSI_FREQ_MAX)


/*===========================================================================*/
/*                          COMMON CONFIGURATIONS                             */
/*===========================================================================*/

/**
 * Pre-calculated configurations for common timeout values at 32 kHz LSI.
 *
 * Format per entry:
 *   _PRESCALER: register value to write to PR (0–6)
 *   _RELOAD:    register value to write to RLR (1–4095)
 *
 * Verification formula (nominal): T_ms = (prescaler_div × reload × 1000) / 32000
 * Worst-case (fastest) timeout:   T_min = T_nominal × (32000 / 47000)
 *
 * ──────────────────────────────────────────────────────────────────────────
 *  Config          PR reg  Div  Reload   Nominal    Worst-case (47 kHz LSI)
 * ──────────────────────────────────────────────────────────────────────────
 *  100 ms          0x4     64    50       100 ms     ~68 ms
 *  500 ms          0x4     64   250       500 ms    ~340 ms
 *  1 second        0x4     64   500      1000 ms    ~681 ms
 *  2 seconds       0x4     64  1000      2000 ms   ~1361 ms
 *  5 seconds       0x5    128  1250      5000 ms   ~3404 ms
 *  10 seconds      0x5    128  2500     10000 ms   ~6809 ms
 * ──────────────────────────────────────────────────────────────────────────
 *
 * RECOMMENDED for this project (FreeRTOS 3-thread):
 *   Use IWDG_CONFIG_1S — 1 second nominal, ~681 ms worst-case.
 *   Feed every 500 ms from supervisor (Part 8.3 pattern).
 *   500 ms feed interval < 681 ms worst-case timeout → safe margin.
 */

/* 100 ms timeout: PR=4 (/64), RL=50 */
#define IWDG_CONFIG_100MS_PRESCALER     IWDG_PRESCALER_64   /* 0x4U */
#define IWDG_CONFIG_100MS_RELOAD        50U

/* 500 ms timeout: PR=4 (/64), RL=250 */
#define IWDG_CONFIG_500MS_PRESCALER     IWDG_PRESCALER_64   /* 0x4U */
#define IWDG_CONFIG_500MS_RELOAD        250U

/* 1 second timeout: PR=4 (/64), RL=500
 * ← RECOMMENDED for FreeRTOS 3-thread application */
#define IWDG_CONFIG_1S_PRESCALER        IWDG_PRESCALER_64   /* 0x4U */
#define IWDG_CONFIG_1S_RELOAD           500U

/* 2 second timeout: PR=4 (/64), RL=1000 */
#define IWDG_CONFIG_2S_PRESCALER        IWDG_PRESCALER_64   /* 0x4U */
#define IWDG_CONFIG_2S_RELOAD           1000U

/* 5 second timeout: PR=5 (/128), RL=1250 */
#define IWDG_CONFIG_5S_PRESCALER        IWDG_PRESCALER_128  /* 0x5U */
#define IWDG_CONFIG_5S_RELOAD           1250U

/* 10 second timeout: PR=5 (/128), RL=2500 */
#define IWDG_CONFIG_10S_PRESCALER       IWDG_PRESCALER_128  /* 0x5U */
#define IWDG_CONFIG_10S_RELOAD          2500U


#endif /* IWDG_REGS_H */

/***************************** END OF FILE ************************************/
