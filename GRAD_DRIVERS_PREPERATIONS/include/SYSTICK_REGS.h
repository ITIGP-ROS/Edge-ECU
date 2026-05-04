#ifndef SYSTICK_REGS_H
#define SYSTICK_REGS_H

#include "STD_TYPES.h"


/* ----------------------------------------------------------------
 * Base address — ARMv7-M SysTick (PM0214 §4.5)
 * UL suffix required per MISRA Rule 7.2
 * ---------------------------------------------------------------- */
#define SYSTICK_BASE_ADDR    0xE000E010UL

/* Peripheral pointer macro — renamed from "SysTick" to "SYSTICK"
 * to avoid collision with CMSIS-defined SysTick symbol,
 * and to match project ALL_CAPS convention (TIM2, NVIC, UART1…) */
#define SYSTICK              ((volatile SysTick_REGS_t*)SYSTICK_BASE_ADDR)


typedef struct
{
    /* ============================================================
     * 0x000 : STK_CTRL  (SysTick Control and Status Register)
     * ============================================================ */
    union {
        volatile uint32_t STK_CTRL;            /* 0x00 — volatile here governs HW access */
        struct {
            uint32_t ENABLE     : 1;           /* [0]      */
            uint32_t TICKINT    : 1;           /* [1]      */
            uint32_t CLKSOURCE  : 1;           /* [2]      */
            uint32_t RESERVED0  : 13;          /* [15:3]   */
            uint32_t COUNTFLAG  : 1;           /* [16]     */
            uint32_t RESERVED1  : 15;          /* [31:17]  */
        } STK_CTRL_b;
    };

    /* ============================================================
     * 0x004 : STK_LOAD (SysTick Reload Value Register)
     * ============================================================ */
    union {
        volatile uint32_t STK_LOAD;            /* 0x04 */
        struct {
            uint32_t RELOAD     : 24;          /* [23:0]   */
            uint32_t RESERVED   : 8;           /* [31:24]  */
        } STK_LOAD_b;
    };

    /* ============================================================
     * 0x008 : STK_VAL (SysTick Current Value Register)
     * ============================================================ */
    union {
        volatile uint32_t STK_VAL;             /* 0x08 */
        struct {
            uint32_t CURRENT    : 24;          /* [23:0]   */
            uint32_t RESERVED   : 8;           /* [31:24]  */
        } STK_VAL_b;
    };

    /* ============================================================
     * 0x00C : STK_CALIB (SysTick Calibration Register — read-only)
     * ============================================================ */
    union {
        volatile uint32_t STK_CALIB;           /* 0x0C */
        struct {
            uint32_t TENMS      : 24;          /* [23:0]   */
            uint32_t RESERVED0  : 6;           /* [29:24]  */
            uint32_t SKEW       : 1;           /* [30]     */
            uint32_t NOREF      : 1;           /* [31]     */
        } STK_CALIB_b;
    };

} SysTick_REGS_t;


#endif /* SYSTICK_REGS_H */
