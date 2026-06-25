/*****************************************************************************
 * TIM_REGS.h — Register Map for General Purpose Timers (TIM2–TIM5)
 *              STM32F401CC, Reference Manual RM0368
 *
 * Target Timers : TIM2, TIM3, TIM4, TIM5 (APB1 bus)
 * All four share an IDENTICAL register layout — one struct covers all.
 *
 * T-03: TIM2 and TIM5 are 32-bit counter timers (CNT, ARR, CCRx are 32-bit).
 *       TIM3 and TIM4 are 16-bit counters (upper 16 bits of CNT, ARR, CCRx
 *       are reserved and read as zero). The struct uses uint32_t for all
 *       registers — correct for register-width access on AHB/APB.
 *       Do NOT make separate structs; the hardware layout is identical.
 *
 * T-01: PSC and ARR calculation for 100 Hz tick at 84 MHz system clock:
 *       TIM2 is on APB1. APB1 prescaler = /2 → APB1 bus clock = 42 MHz.
 *       When APB1 prescaler ≠ 1, timer clock = 2 × APB1 = 84 MHz.
 *         PSC = 8399   → tick rate = 84 000 000 / (8399 + 1) = 10 000 Hz
 *         ARR = 99     → period   = 10 000 / (99 + 1) = 100 Hz  ✓
 *
 * T-02: PSC is buffered but loads immediately on UG event.
 *       ARR with ARPE=1 is double-buffered (loads on next update event).
 *       Correct init sequence: write PSC, write ARR, set EGR.UG = 1 to
 *       force an update event and load both shadow registers, THEN enable
 *       CEN.  If EGR.UG is skipped, the first period may be incorrect.
 *
 * T-04: SR.UIF must be cleared by software inside the ISR.
 *       Hardware sets UIF; software clears it by writing 0 to the bit.
 *       Writing 1 has no effect (rc_w0 — read/clear-write-0 type).
 *       If you forget to clear UIF, the ISR re-enters immediately.
 *
 * T-05: EGR is a write-only register. Reading it returns 0x0000.
 *       Do NOT read-modify-write. Write the bit directly:
 *         TIM2->EGR.ALL = (1U << 0U);
 *
 *****************************************************************************/

#ifndef TIM_REGS_H
#define TIM_REGS_H

#include "STD_TYPES.h"

/*═══════════════════════════════════════════════════════════════════════════
 * BASE ADDRESSES — RM0368 §2.3 Memory Map
 *═══════════════════════════════════════════════════════════════════════════*/

/* T-06: All base addresses use UL suffix per MISRA Rule 7.2 to ensure
 *       unsigned long type in pointer cast macros.                          */
#define TIM2_BASE_ADDR   0x40000000UL
#define TIM3_BASE_ADDR   0x40000400UL
#define TIM4_BASE_ADDR   0x40000800UL
#define TIM5_BASE_ADDR   0x40000C00UL

/*═══════════════════════════════════════════════════════════════════════════
 * 100 Hz TIMER CONSTANTS — See T-01 derivation above
 *═══════════════════════════════════════════════════════════════════════════*/

/* Timer input clock = 84 MHz (APB1 timer clock when APB1 prescaler ≠ 1)
 * Desired output    = 100 Hz
 *
 * tick_rate = 84 000 000 / (PSC + 1) = 84 000 000 / 8400 = 10 000 Hz
 * period    = tick_rate  / (ARR + 1) = 10 000     / 100   = 100 Hz       */
#define TIM_PSC_100HZ_84MHZ   8399U    /* Prescaler value (PSC register)  */
#define TIM_ARR_100HZ_84MHZ   99U      /* Auto-reload value (ARR register)*/
#define TIM_INPUT_CLOCK_HZ    84000000UL  /* Timer clock after APB1 mult  */

/*═══════════════════════════════════════════════════════════════════════════
 * REGISTER DEFINITIONS — RM0368 §14.4 (General Purpose Timer Register Map)
 *
 * Each register is a union:
 *   .ALL   — volatile uint32_t for full-word access
 *   .BITS  — bitfield struct for named field access
 *
 * T-07: volatile is on ALL only, NOT on individual bitfields.
 *       volatile on bitfields is non-standard in C99/C11 and causes
 *       implementation-defined behavior across compilers.
 *═══════════════════════════════════════════════════════════════════════════*/

/*---------------------------------------------------------------------------
 * CR1 — Control Register 1 (Offset 0x00)
 *---------------------------------------------------------------------------*/
typedef union
{
    volatile uint32_t ALL;
    struct
    {
        uint32_t CEN       : 1U;   /* Bit  0     : Counter Enable                */
        uint32_t UDIS      : 1U;   /* Bit  1     : Update Disable                */
        uint32_t URS       : 1U;   /* Bit  2     : Update Request Source          */
        uint32_t OPM       : 1U;   /* Bit  3     : One Pulse Mode                */
        uint32_t DIR       : 1U;   /* Bit  4     : Direction (0=up, 1=down)       */
        uint32_t CMS       : 2U;   /* Bits 6:5   : Center-aligned Mode Selection  */
        uint32_t ARPE      : 1U;   /* Bit  7     : Auto-Reload Preload Enable     */
        uint32_t CKD       : 2U;   /* Bits 9:8   : Clock Division                 */
        uint32_t RESERVED  : 22U;  /* Bits 31:10 : Reserved, must be kept at 0    */
    } BITS;
} TIM_CR1_t;

/*---------------------------------------------------------------------------
 * CR2 — Control Register 2 (Offset 0x04)
 *---------------------------------------------------------------------------*/
typedef union
{
    volatile uint32_t ALL;
    struct
    {
        uint32_t RESERVED0 : 3U;   /* Bits 2:0   : Reserved                       */
        uint32_t CCDS      : 1U;   /* Bit  3     : Capture/Compare DMA Selection   */
        uint32_t MMS       : 3U;   /* Bits 6:4   : Master Mode Selection           */
        uint32_t TI1S      : 1U;   /* Bit  7     : TI1 Selection                   */
        uint32_t RESERVED1 : 24U;  /* Bits 31:8  : Reserved, must be kept at 0     */
    } BITS;
} TIM_CR2_t;

/*---------------------------------------------------------------------------
 * SMCR — Slave Mode Control Register (Offset 0x08)
 *---------------------------------------------------------------------------*/
typedef union
{
    volatile uint32_t ALL;
    struct
    {
        uint32_t SMS       : 3U;   /* Bits 2:0   : Slave Mode Selection            */
        uint32_t RESERVED0 : 1U;   /* Bit  3     : Reserved                        */
        uint32_t TS        : 3U;   /* Bits 6:4   : Trigger Selection               */
        uint32_t MSM       : 1U;   /* Bit  7     : Master/Slave Mode               */
        uint32_t ETF       : 4U;   /* Bits 11:8  : External Trigger Filter          */
        uint32_t ETPS      : 2U;   /* Bits 13:12 : External Trigger Prescaler       */
        uint32_t ECE       : 1U;   /* Bit  14    : External Clock Enable            */
        uint32_t ETP       : 1U;   /* Bit  15    : External Trigger Polarity        */
        uint32_t RESERVED1 : 16U;  /* Bits 31:16 : Reserved, must be kept at 0      */
    } BITS;
} TIM_SMCR_t;

/*---------------------------------------------------------------------------
 * DIER — DMA/Interrupt Enable Register (Offset 0x0C)
 *
 * UIE (bit 0) is the one that fires the 100 Hz update interrupt.
 *---------------------------------------------------------------------------*/
typedef union
{
    volatile uint32_t ALL;
    struct
    {
        uint32_t UIE       : 1U;   /* Bit  0     : Update Interrupt Enable         */
        uint32_t CC1IE     : 1U;   /* Bit  1     : Capture/Compare 1 Int Enable    */
        uint32_t CC2IE     : 1U;   /* Bit  2     : Capture/Compare 2 Int Enable    */
        uint32_t CC3IE     : 1U;   /* Bit  3     : Capture/Compare 3 Int Enable    */
        uint32_t CC4IE     : 1U;   /* Bit  4     : Capture/Compare 4 Int Enable    */
        uint32_t RESERVED0 : 1U;   /* Bit  5     : Reserved                        */
        uint32_t TIE       : 1U;   /* Bit  6     : Trigger Interrupt Enable         */
        uint32_t RESERVED1 : 1U;   /* Bit  7     : Reserved                        */
        uint32_t UDE       : 1U;   /* Bit  8     : Update DMA Request Enable        */
        uint32_t CC1DE     : 1U;   /* Bit  9     : CC1 DMA Request Enable           */
        uint32_t CC2DE     : 1U;   /* Bit  10    : CC2 DMA Request Enable           */
        uint32_t CC3DE     : 1U;   /* Bit  11    : CC3 DMA Request Enable           */
        uint32_t CC4DE     : 1U;   /* Bit  12    : CC4 DMA Request Enable           */
        uint32_t RESERVED2 : 1U;   /* Bit  13    : Reserved                        */
        uint32_t TDE       : 1U;   /* Bit  14    : Trigger DMA Request Enable       */
        uint32_t RESERVED3 : 17U;  /* Bits 31:15 : Reserved, must be kept at 0      */
    } BITS;
} TIM_DIER_t;

/*---------------------------------------------------------------------------
 * SR — Status Register (Offset 0x10)
 *
 * T-04: UIF is rc_w0 — read/clear-write-0. Writing 1 has no effect.
 *       You MUST clear UIF inside the ISR or it re-enters immediately.
 *       Clear with: TIMx->SR.ALL = ~(1U << 0U);
 *       Or:         TIMx->SR.BITS.UIF = 0U;
 *       (The rc_w0 nature means writing 0 clears, writing 1 is ignored.)
 *---------------------------------------------------------------------------*/
typedef union
{
    volatile uint32_t ALL;
    struct
    {
        uint32_t UIF       : 1U;   /* Bit  0     : Update Interrupt Flag (rc_w0)   */
        uint32_t CC1IF     : 1U;   /* Bit  1     : Capture/Compare 1 Int Flag      */
        uint32_t CC2IF     : 1U;   /* Bit  2     : Capture/Compare 2 Int Flag      */
        uint32_t CC3IF     : 1U;   /* Bit  3     : Capture/Compare 3 Int Flag      */
        uint32_t CC4IF     : 1U;   /* Bit  4     : Capture/Compare 4 Int Flag      */
        uint32_t RESERVED0 : 1U;   /* Bit  5     : Reserved                        */
        uint32_t TIF       : 1U;   /* Bit  6     : Trigger Interrupt Flag           */
        uint32_t RESERVED1 : 2U;   /* Bits 8:7   : Reserved                        */
        uint32_t CC1OF     : 1U;   /* Bit  9     : CC1 Overcapture Flag             */
        uint32_t CC2OF     : 1U;   /* Bit  10    : CC2 Overcapture Flag             */
        uint32_t CC3OF     : 1U;   /* Bit  11    : CC3 Overcapture Flag             */
        uint32_t CC4OF     : 1U;   /* Bit  12    : CC4 Overcapture Flag             */
        uint32_t RESERVED2 : 19U;  /* Bits 31:13 : Reserved, must be kept at 0      */
    } BITS;
} TIM_SR_t;

/*---------------------------------------------------------------------------
 * EGR — Event Generation Register (Offset 0x14)
 *
 * T-05: EGR is WRITE-ONLY. Reading returns 0x0000.
 *       Do NOT read-modify-write this register.
 *       Write bits directly: TIMx->EGR.ALL = (1U << 0U);
 *---------------------------------------------------------------------------*/
typedef union
{
    volatile uint32_t ALL;
    struct
    {
        uint32_t UG        : 1U;   /* Bit  0     : Update Generation (SW trigger)  */
        uint32_t CC1G      : 1U;   /* Bit  1     : CC1 Generation                  */
        uint32_t CC2G      : 1U;   /* Bit  2     : CC2 Generation                  */
        uint32_t CC3G      : 1U;   /* Bit  3     : CC3 Generation                  */
        uint32_t CC4G      : 1U;   /* Bit  4     : CC4 Generation                  */
        uint32_t RESERVED0 : 1U;   /* Bit  5     : Reserved                        */
        uint32_t TG        : 1U;   /* Bit  6     : Trigger Generation               */
        uint32_t RESERVED1 : 25U;  /* Bits 31:7  : Reserved, must be kept at 0      */
    } BITS;
} TIM_EGR_t;

/*---------------------------------------------------------------------------
 * CCMR1 — Capture/Compare Mode Register 1 (Offset 0x18)
 *
 * T-08: CCMR1 has two interpretations depending on CC1S/CC2S:
 *       Output Compare mode vs Input Capture mode. This definition
 *       covers the Output Compare layout. For Input Capture use,
 *       access via .ALL and manual bit manipulation.
 *---------------------------------------------------------------------------*/
typedef union
{
    volatile uint32_t ALL;
    struct
    {
        /* Channel 1 */
        uint32_t CC1S      : 2U;   /* Bits 1:0   : Capture/Compare 1 Selection     */
        uint32_t OC1FE     : 1U;   /* Bit  2     : Output Compare 1 Fast Enable    */
        uint32_t OC1PE     : 1U;   /* Bit  3     : Output Compare 1 Preload Enable */
        uint32_t OC1M      : 3U;   /* Bits 6:4   : Output Compare 1 Mode           */
        uint32_t OC1CE     : 1U;   /* Bit  7     : Output Compare 1 Clear Enable   */
        /* Channel 2 */
        uint32_t CC2S      : 2U;   /* Bits 9:8   : Capture/Compare 2 Selection     */
        uint32_t OC2FE     : 1U;   /* Bit  10    : Output Compare 2 Fast Enable    */
        uint32_t OC2PE     : 1U;   /* Bit  11    : Output Compare 2 Preload Enable */
        uint32_t OC2M      : 3U;   /* Bits 14:12 : Output Compare 2 Mode           */
        uint32_t OC2CE     : 1U;   /* Bit  15    : Output Compare 2 Clear Enable   */
        uint32_t RESERVED  : 16U;  /* Bits 31:16 : Reserved, must be kept at 0      */
    } BITS;
} TIM_CCMR1_t;

/*---------------------------------------------------------------------------
 * CCMR2 — Capture/Compare Mode Register 2 (Offset 0x1C)
 *
 * Same dual-mode note as CCMR1 (T-08).
 * This definition covers Output Compare layout for channels 3 and 4.
 *---------------------------------------------------------------------------*/
typedef union
{
    volatile uint32_t ALL;
    struct
    {
        /* Channel 3 */
        uint32_t CC3S      : 2U;   /* Bits 1:0   : Capture/Compare 3 Selection     */
        uint32_t OC3FE     : 1U;   /* Bit  2     : Output Compare 3 Fast Enable    */
        uint32_t OC3PE     : 1U;   /* Bit  3     : Output Compare 3 Preload Enable */
        uint32_t OC3M      : 3U;   /* Bits 6:4   : Output Compare 3 Mode           */
        uint32_t OC3CE     : 1U;   /* Bit  7     : Output Compare 3 Clear Enable   */
        /* Channel 4 */
        uint32_t CC4S      : 2U;   /* Bits 9:8   : Capture/Compare 4 Selection     */
        uint32_t OC4FE     : 1U;   /* Bit  10    : Output Compare 4 Fast Enable    */
        uint32_t OC4PE     : 1U;   /* Bit  11    : Output Compare 4 Preload Enable */
        uint32_t OC4M      : 3U;   /* Bits 14:12 : Output Compare 4 Mode           */
        uint32_t OC4CE     : 1U;   /* Bit  15    : Output Compare 4 Clear Enable   */
        uint32_t RESERVED  : 16U;  /* Bits 31:16 : Reserved, must be kept at 0      */
    } BITS;
} TIM_CCMR2_t;

/*---------------------------------------------------------------------------
 * CCER — Capture/Compare Enable Register (Offset 0x20)
 *---------------------------------------------------------------------------*/
typedef union
{
    volatile uint32_t ALL;
    struct
    {
        uint32_t CC1E      : 1U;   /* Bit  0     : Capture/Compare 1 Output Enable */
        uint32_t CC1P      : 1U;   /* Bit  1     : CC1 Polarity                    */
        uint32_t RESERVED0 : 1U;   /* Bit  2     : Reserved                        */
        uint32_t CC1NP     : 1U;   /* Bit  3     : CC1 Complementary Output Pol    */
        uint32_t CC2E      : 1U;   /* Bit  4     : Capture/Compare 2 Output Enable */
        uint32_t CC2P      : 1U;   /* Bit  5     : CC2 Polarity                    */
        uint32_t RESERVED1 : 1U;   /* Bit  6     : Reserved                        */
        uint32_t CC2NP     : 1U;   /* Bit  7     : CC2 Complementary Output Pol    */
        uint32_t CC3E      : 1U;   /* Bit  8     : Capture/Compare 3 Output Enable */
        uint32_t CC3P      : 1U;   /* Bit  9     : CC3 Polarity                    */
        uint32_t RESERVED2 : 1U;   /* Bit  10    : Reserved                        */
        uint32_t CC3NP     : 1U;   /* Bit  11    : CC3 Complementary Output Pol    */
        uint32_t CC4E      : 1U;   /* Bit  12    : Capture/Compare 4 Output Enable */
        uint32_t CC4P      : 1U;   /* Bit  13    : CC4 Polarity                    */
        uint32_t RESERVED3 : 1U;   /* Bit  14    : Reserved                        */
        uint32_t CC4NP     : 1U;   /* Bit  15    : CC4 Complementary Output Pol    */
        uint32_t RESERVED4 : 16U;  /* Bits 31:16 : Reserved, must be kept at 0      */
    } BITS;
} TIM_CCER_t;

/*---------------------------------------------------------------------------
 * CNT — Counter Register (Offset 0x24)
 *
 * T-03: 32-bit on TIM2/TIM5; upper 16 bits reserved on TIM3/TIM4.
 *---------------------------------------------------------------------------*/
typedef union
{
    volatile uint32_t ALL;
    struct
    {
        uint32_t CNT       : 32U;  /* Bits 31:0  : Counter Value                   */
    } BITS;
} TIM_CNT_t;

/*---------------------------------------------------------------------------
 * PSC — Prescaler Register (Offset 0x28)
 *
 * T-02: PSC is buffered. New value loads into the shadow register at each
 *       update event — or immediately when EGR.UG is set.
 *       PSC value used = PSC[15:0]; counter clock = CK_PSC / (PSC + 1).
 *---------------------------------------------------------------------------*/
typedef union
{
    volatile uint32_t ALL;
    struct
    {
        uint32_t PSC       : 16U;  /* Bits 15:0  : Prescaler Value                 */
        uint32_t RESERVED  : 16U;  /* Bits 31:16 : Reserved, must be kept at 0      */
    } BITS;
} TIM_PSC_t;

/*---------------------------------------------------------------------------
 * ARR — Auto-Reload Register (Offset 0x2C)
 *
 * T-02: When ARPE=1, ARR is double-buffered. Written value goes to the
 *       preload register and transfers to the shadow on the next update
 *       event. Set EGR.UG = 1 after writing to force immediate shadow load.
 * T-03: 32-bit on TIM2/TIM5; upper 16 bits reserved on TIM3/TIM4.
 *---------------------------------------------------------------------------*/
typedef union
{
    volatile uint32_t ALL;
    struct
    {
        uint32_t ARR       : 32U;  /* Bits 31:0  : Auto-Reload Value               */
    } BITS;
} TIM_ARR_t;

/*---------------------------------------------------------------------------
 * CCR1 — Capture/Compare Register 1 (Offset 0x34)
 * T-03: 32-bit on TIM2/TIM5; upper 16 bits reserved on TIM3/TIM4.
 *---------------------------------------------------------------------------*/
typedef union
{
    volatile uint32_t ALL;
    struct
    {
        uint32_t CCR1      : 32U;  /* Bits 31:0  : Capture/Compare 1 Value         */
    } BITS;
} TIM_CCR1_t;

/*---------------------------------------------------------------------------
 * CCR2 — Capture/Compare Register 2 (Offset 0x38)
 * T-03: 32-bit on TIM2/TIM5; upper 16 bits reserved on TIM3/TIM4.
 *---------------------------------------------------------------------------*/
typedef union
{
    volatile uint32_t ALL;
    struct
    {
        uint32_t CCR2      : 32U;  /* Bits 31:0  : Capture/Compare 2 Value         */
    } BITS;
} TIM_CCR2_t;

/*---------------------------------------------------------------------------
 * CCR3 — Capture/Compare Register 3 (Offset 0x3C)
 * T-03: 32-bit on TIM2/TIM5; upper 16 bits reserved on TIM3/TIM4.
 *---------------------------------------------------------------------------*/
typedef union
{
    volatile uint32_t ALL;
    struct
    {
        uint32_t CCR3      : 32U;  /* Bits 31:0  : Capture/Compare 3 Value         */
    } BITS;
} TIM_CCR3_t;

/*---------------------------------------------------------------------------
 * CCR4 — Capture/Compare Register 4 (Offset 0x40)
 * T-03: 32-bit on TIM2/TIM5; upper 16 bits reserved on TIM3/TIM4.
 *---------------------------------------------------------------------------*/
typedef union
{
    volatile uint32_t ALL;
    struct
    {
        uint32_t CCR4      : 32U;  /* Bits 31:0  : Capture/Compare 4 Value         */
    } BITS;
} TIM_CCR4_t;

/*---------------------------------------------------------------------------
 * DCR — DMA Control Register (Offset 0x48)
 *---------------------------------------------------------------------------*/
typedef union
{
    volatile uint32_t ALL;
    struct
    {
        uint32_t DBA       : 5U;   /* Bits 4:0   : DMA Base Address                */
        uint32_t RESERVED0 : 3U;   /* Bits 7:5   : Reserved                        */
        uint32_t DBL       : 5U;   /* Bits 12:8  : DMA Burst Length                */
        uint32_t RESERVED1 : 19U;  /* Bits 31:13 : Reserved, must be kept at 0      */
    } BITS;
} TIM_DCR_t;

/*---------------------------------------------------------------------------
 * DMAR — DMA Address for Full Transfer (Offset 0x4C)
 *---------------------------------------------------------------------------*/
typedef union
{
    volatile uint32_t ALL;
    struct
    {
        uint32_t DMAB      : 16U;  /* Bits 15:0  : DMA register for burst accesses */
        uint32_t RESERVED  : 16U;  /* Bits 31:16 : Reserved, must be kept at 0      */
    } BITS;
} TIM_DMAR_t;

/*═══════════════════════════════════════════════════════════════════════════
 * PERIPHERAL REGISTER STRUCTURE — RM0368 §14.4 Register Map
 *
 * One typedef covers TIM2, TIM3, TIM4, TIM5 (identical layout).
 * Byte offsets are noted in comments. Gaps are padded with RESERVED_xx.
 *
 * T-09: Offset 0x30 is a gap between ARR (0x2C) and CCR1 (0x34).
 *       Offset 0x44 is a gap between CCR4 (0x40) and DCR (0x48).
 *       Offset 0x50 is OR register (TIM2/TIM5 only, not on TIM3/TIM4
 *       in STM32F401CC) — padded as reserved for uniform struct size.
 *═══════════════════════════════════════════════════════════════════════════*/

typedef struct
{
    TIM_CR1_t    CR1;           /* Offset 0x00 : Control Register 1              */
    TIM_CR2_t    CR2;           /* Offset 0x04 : Control Register 2              */
    TIM_SMCR_t   SMCR;         /* Offset 0x08 : Slave Mode Control Register     */
    TIM_DIER_t   DIER;         /* Offset 0x0C : DMA/Interrupt Enable Register   */
    TIM_SR_t     SR;            /* Offset 0x10 : Status Register                 */
    TIM_EGR_t    EGR;          /* Offset 0x14 : Event Generation Register (W/O) */
    TIM_CCMR1_t  CCMR1;        /* Offset 0x18 : Capture/Compare Mode Register 1 */
    TIM_CCMR2_t  CCMR2;        /* Offset 0x1C : Capture/Compare Mode Register 2 */
    TIM_CCER_t   CCER;         /* Offset 0x20 : Capture/Compare Enable Register */
    TIM_CNT_t    CNT;          /* Offset 0x24 : Counter Register                */
    TIM_PSC_t    PSC;          /* Offset 0x28 : Prescaler Register              */
    TIM_ARR_t    ARR;          /* Offset 0x2C : Auto-Reload Register            */
    volatile uint32_t RESERVED_30;  /* Offset 0x30 : Reserved (gap in map)      */
    TIM_CCR1_t   CCR1;         /* Offset 0x34 : Capture/Compare Register 1      */
    TIM_CCR2_t   CCR2;         /* Offset 0x38 : Capture/Compare Register 2      */
    TIM_CCR3_t   CCR3;         /* Offset 0x3C : Capture/Compare Register 3      */
    TIM_CCR4_t   CCR4;         /* Offset 0x40 : Capture/Compare Register 4      */
    volatile uint32_t RESERVED_44;  /* Offset 0x44 : Reserved (gap in map)      */
    TIM_DCR_t    DCR;          /* Offset 0x48 : DMA Control Register            */
    TIM_DMAR_t   DMAR;         /* Offset 0x4C : DMA Address for Full Transfer   */
} TIM_REGS_t;

/*═══════════════════════════════════════════════════════════════════════════
 * PERIPHERAL POINTER MACROS
 *
 * T-10: Cast base address to volatile pointer to TIM_REGS_t.
 *       volatile on the pointer target ensures all register accesses
 *       through TIMx->REG.ALL are treated as volatile by the compiler.
 *═══════════════════════════════════════════════════════════════════════════*/

#define TIM2    ((volatile TIM_REGS_t *)TIM2_BASE_ADDR)
#define TIM3    ((volatile TIM_REGS_t *)TIM3_BASE_ADDR)
#define TIM4    ((volatile TIM_REGS_t *)TIM4_BASE_ADDR)
#define TIM5    ((volatile TIM_REGS_t *)TIM5_BASE_ADDR)

/*═══════════════════════════════════════════════════════════════════════════
 * CONVENIENCE BIT MASKS — for use with .ALL register access
 *
 * T-11: All masks use U suffix per MISRA Rule 7.2.
 *       Shift amounts also carry U suffix to avoid implicit conversions.
 *═══════════════════════════════════════════════════════════════════════════*/

/* CR1 bit masks */
#define TIM_CR1_CEN_MASK       (1U << 0U)    /* Counter Enable              */
#define TIM_CR1_UDIS_MASK      (1U << 1U)    /* Update Disable              */
#define TIM_CR1_URS_MASK       (1U << 2U)    /* Update Request Source        */
#define TIM_CR1_OPM_MASK       (1U << 3U)    /* One Pulse Mode              */
#define TIM_CR1_DIR_MASK       (1U << 4U)    /* Direction                   */
#define TIM_CR1_CMS_MASK       (3U << 5U)    /* Center-aligned Mode (2 bits)*/
#define TIM_CR1_ARPE_MASK      (1U << 7U)    /* Auto-Reload Preload Enable  */
#define TIM_CR1_CKD_MASK       (3U << 8U)    /* Clock Division (2 bits)     */

/* DIER bit masks */
#define TIM_DIER_UIE_MASK      (1U << 0U)    /* Update Interrupt Enable     */
#define TIM_DIER_CC1IE_MASK    (1U << 1U)    /* CC1 Interrupt Enable        */
#define TIM_DIER_CC2IE_MASK    (1U << 2U)    /* CC2 Interrupt Enable        */
#define TIM_DIER_CC3IE_MASK    (1U << 3U)    /* CC3 Interrupt Enable        */
#define TIM_DIER_CC4IE_MASK    (1U << 4U)    /* CC4 Interrupt Enable        */
#define TIM_DIER_TIE_MASK     (1U << 6U)    /* Trigger Interrupt Enable    */
#define TIM_DIER_UDE_MASK      (1U << 8U)    /* Update DMA Request Enable   */
#define TIM_DIER_CC1DE_MASK    (1U << 9U)    /* CC1 DMA Request Enable      */
#define TIM_DIER_CC2DE_MASK    (1U << 10U)   /* CC2 DMA Request Enable      */
#define TIM_DIER_CC3DE_MASK    (1U << 11U)   /* CC3 DMA Request Enable      */
#define TIM_DIER_CC4DE_MASK    (1U << 12U)   /* CC4 DMA Request Enable      */
#define TIM_DIER_TDE_MASK     (1U << 14U)   /* Trigger DMA Request Enable  */

/* SR bit masks */
#define TIM_SR_UIF_MASK        (1U << 0U)    /* Update Interrupt Flag        */
#define TIM_SR_CC1IF_MASK      (1U << 1U)    /* CC1 Interrupt Flag           */
#define TIM_SR_CC2IF_MASK      (1U << 2U)    /* CC2 Interrupt Flag           */
#define TIM_SR_CC3IF_MASK      (1U << 3U)    /* CC3 Interrupt Flag           */
#define TIM_SR_CC4IF_MASK      (1U << 4U)    /* CC4 Interrupt Flag           */
#define TIM_SR_TIF_MASK        (1U << 6U)    /* Trigger Interrupt Flag       */
#define TIM_SR_CC1OF_MASK      (1U << 9U)    /* CC1 Overcapture Flag         */
#define TIM_SR_CC2OF_MASK      (1U << 10U)   /* CC2 Overcapture Flag         */
#define TIM_SR_CC3OF_MASK      (1U << 11U)   /* CC3 Overcapture Flag         */
#define TIM_SR_CC4OF_MASK      (1U << 12U)   /* CC4 Overcapture Flag         */

/* EGR bit masks — T-05: Write-only register, never read-modify-write */
#define TIM_EGR_UG_MASK        (1U << 0U)    /* Update Generation            */
#define TIM_EGR_CC1G_MASK      (1U << 1U)    /* CC1 Generation               */
#define TIM_EGR_CC2G_MASK      (1U << 2U)    /* CC2 Generation               */
#define TIM_EGR_CC3G_MASK      (1U << 3U)    /* CC3 Generation               */
#define TIM_EGR_CC4G_MASK      (1U << 4U)    /* CC4 Generation               */
#define TIM_EGR_TG_MASK        (1U << 6U)    /* Trigger Generation           */

/* CCER bit masks */
#define TIM_CCER_CC1E_MASK     (1U << 0U)    /* CC1 Output Enable            */
#define TIM_CCER_CC1P_MASK     (1U << 1U)    /* CC1 Polarity                 */
#define TIM_CCER_CC1NP_MASK    (1U << 3U)    /* CC1 Complementary Polarity   */
#define TIM_CCER_CC2E_MASK     (1U << 4U)    /* CC2 Output Enable            */
#define TIM_CCER_CC2P_MASK     (1U << 5U)    /* CC2 Polarity                 */
#define TIM_CCER_CC2NP_MASK    (1U << 7U)    /* CC2 Complementary Polarity   */
#define TIM_CCER_CC3E_MASK     (1U << 8U)    /* CC3 Output Enable            */
#define TIM_CCER_CC3P_MASK     (1U << 9U)    /* CC3 Polarity                 */
#define TIM_CCER_CC3NP_MASK    (1U << 11U)   /* CC3 Complementary Polarity   */
#define TIM_CCER_CC4E_MASK     (1U << 12U)   /* CC4 Output Enable            */
#define TIM_CCER_CC4P_MASK     (1U << 13U)   /* CC4 Polarity                 */
#define TIM_CCER_CC4NP_MASK    (1U << 15U)   /* CC4 Complementary Polarity   */

/* SMCR bit masks */
#define TIM_SMCR_SMS_MASK      (7U << 0U)    /* Slave Mode Selection (3 bits)*/
#define TIM_SMCR_TS_MASK       (7U << 4U)    /* Trigger Selection (3 bits)   */
#define TIM_SMCR_MSM_MASK      (1U << 7U)    /* Master/Slave Mode            */
#define TIM_SMCR_ETF_MASK      (0xFU << 8U)  /* External Trigger Filter      */
#define TIM_SMCR_ETPS_MASK     (3U << 12U)   /* External Trigger Prescaler   */
#define TIM_SMCR_ECE_MASK      (1U << 14U)   /* External Clock Enable        */
#define TIM_SMCR_ETP_MASK      (1U << 15U)   /* External Trigger Polarity    */

/* DCR bit masks */
#define TIM_DCR_DBA_MASK       (0x1FU << 0U) /* DMA Base Address (5 bits)    */
#define TIM_DCR_DBL_MASK       (0x1FU << 8U) /* DMA Burst Length (5 bits)    */

/* PSC mask — full 16-bit prescaler field */
#define TIM_PSC_MASK           0xFFFFU        /* Prescaler value [15:0]       */

/* ARR mask — full 32-bit for TIM2/TIM5, only lower 16-bit valid TIM3/TIM4 */
#define TIM_ARR_MASK_32BIT     0xFFFFFFFFUL   /* TIM2/TIM5 full 32-bit       */
#define TIM_ARR_MASK_16BIT     0x0000FFFFUL   /* TIM3/TIM4 lower 16-bit only */

#endif /* TIM_REGS_H */