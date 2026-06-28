#ifndef ADC_REGS_H_
#define ADC_REGS_H_

#include "STD_TYPES.h"

/* ============================================================
 *  ADC Base Addresses — STM32F401 (RM0368)
 * ============================================================ */
#define ADC1_BASE_ADDR   0x40012000U
#define ADC_CCR_BASE     0x40012300U   /* Common control register */

/* ============================================================
 *  ADC1 Register Map
 * ============================================================ */

/* ADC Status Register (SR) */
typedef union {
    uint32_t ALL;
    struct {
        uint32_t AWD   : 1;   /* Analog watchdog flag */
        uint32_t EOC   : 1;   /* End of conversion */
        uint32_t JEOC  : 1;   /* Injected channel end of conversion */
        uint32_t JSTRT : 1;   /* Injected channel start flag */
        uint32_t STRT  : 1;   /* Regular channel start flag */
        uint32_t OVR   : 1;   /* Overrun */
        uint32_t RSVD  : 26;
    } BITS;
} ADC_SR_t;

/* ADC Control Register 1 (CR1) */
typedef union {
    uint32_t ALL;
    struct {
        uint32_t AWDCH   : 5;   /* Analog watchdog channel select */
        uint32_t EOCIE   : 1;   /* Interrupt enable for EOC */
        uint32_t AWDIE   : 1;   /* Analog watchdog interrupt enable */
        uint32_t JEOCIE  : 1;   /* Injected channels interrupt enable */
        uint32_t SCAN    : 1;   /* Scan mode */
        uint32_t AWDSGL  : 1;   /* Enable the watchdog on a single channel */
        uint32_t JAUTO   : 1;   /* Automatic injected group conversion */
        uint32_t DISCEN  : 1;   /* Discontinuous mode on regular channels */
        uint32_t JDISCEN : 1;   /* Discontinuous mode on injected channels */
        uint32_t DISCNUM : 3;   /* Discontinuous mode channel count */
        uint32_t RSVD1   : 6;
        uint32_t JAWDEN  : 1;   /* Analog watchdog on injected channels */
        uint32_t AWDEN   : 1;   /* Analog watchdog on regular channels */
        uint32_t RES     : 2;   /* Resolution: 00=12bit 01=10bit 10=8bit 11=6bit */
        uint32_t OVRIE   : 1;   /* Overrun interrupt enable */
        uint32_t RSVD2   : 5;
    } BITS;
} ADC_CR1_t;

/* ADC Control Register 2 (CR2) */
typedef union {
    uint32_t ALL;
    struct {
        uint32_t ADON    : 1;   /* A/D converter ON / OFF */
        uint32_t CONT    : 1;   /* Continuous conversion */
        uint32_t RSVD1   : 6;
        uint32_t DMA     : 1;   /* DMA mode */
        uint32_t DDS     : 1;   /* DMA disable selection */
        uint32_t EOCS    : 1;   /* End of conversion selection */
        uint32_t ALIGN   : 1;   /* Data alignment: 0=right 1=left */
        uint32_t RSVD2   : 4;
        uint32_t JEXTSEL : 4;   /* External trigger for injected channels */
        uint32_t JEXTEN  : 2;   /* External trigger enable for injected */
        uint32_t JSWSTART: 1;   /* Start conversion of injected channels */
        uint32_t RSVD3   : 1;
        uint32_t EXTSEL  : 4;   /* External event select for regular group */
        uint32_t EXTEN   : 2;   /* External trigger enable for regular */
        uint32_t SWSTART : 1;   /* Start conversion of regular channels */
        uint32_t RSVD4   : 1;
    } BITS;
} ADC_CR2_t;

/* ADC Sample Time Register (SMPR) — two registers cover ch0..ch18 */
typedef union { uint32_t ALL; } ADC_SMPR_t;

/* ADC Regular Sequence Register 3 (SQR3) — holds channel for first conversion */
typedef union {
    uint32_t ALL;
    struct {
        uint32_t SQ1   : 5;   /* 1st conversion in regular sequence */
        uint32_t SQ2   : 5;
        uint32_t SQ3   : 5;
        uint32_t SQ4   : 5;
        uint32_t SQ5   : 5;
        uint32_t SQ6   : 5;
        uint32_t RSVD  : 2;
    } BITS;
} ADC_SQR3_t;

/* ADC Regular Sequence Register 1 (SQR1) — holds sequence length */
typedef union {
    uint32_t ALL;
    struct {
        uint32_t SQ13  : 5;
        uint32_t SQ14  : 5;
        uint32_t SQ15  : 5;
        uint32_t SQ16  : 5;
        uint32_t L     : 4;   /* Regular channel sequence length (0=1 conversion) */
        uint32_t RSVD  : 8;
    } BITS;
} ADC_SQR1_t;

/* ADC Data Register (DR) */
typedef union {
    uint32_t ALL;
    struct {
        uint32_t DATA  : 16;
        uint32_t RSVD  : 16;
    } BITS;
} ADC_DR_t;

/* ============================================================
 *  ADC Register Block
 * ============================================================ */
typedef struct {
    volatile ADC_SR_t   SR;       /* 0x00 */
    volatile ADC_CR1_t  CR1;      /* 0x04 */
    volatile ADC_CR2_t  CR2;      /* 0x08 */
    volatile ADC_SMPR_t SMPR1;    /* 0x0C — channels 10..18 */
    volatile ADC_SMPR_t SMPR2;    /* 0x10 — channels 0..9  */
    volatile uint32_t   JOFR1;    /* 0x14 */
    volatile uint32_t   JOFR2;    /* 0x18 */
    volatile uint32_t   JOFR3;    /* 0x1C */
    volatile uint32_t   JOFR4;    /* 0x20 */
    volatile uint32_t   HTR;      /* 0x24 */
    volatile uint32_t   LTR;      /* 0x28 */
    volatile ADC_SQR1_t SQR1;     /* 0x2C */
    volatile uint32_t   SQR2;     /* 0x30 */
    volatile ADC_SQR3_t SQR3;     /* 0x34 */
    volatile uint32_t   JSQR;     /* 0x38 */
    volatile uint32_t   JDR1;     /* 0x3C */
    volatile uint32_t   JDR2;     /* 0x40 */
    volatile uint32_t   JDR3;     /* 0x44 */
    volatile uint32_t   JDR4;     /* 0x48 */
    volatile ADC_DR_t   DR;       /* 0x4C */
} ADC_REGS_t;

/* ============================================================
 *  Convenience Pointer
 * ============================================================ */
#define ADC1   ((volatile ADC_REGS_t *)ADC1_BASE_ADDR)

/* ADC Common Control Register (CCR) — prescaler */
typedef union {
    uint32_t ALL;
    struct {
        uint32_t MULTI  : 5;
        uint32_t RSVD1  : 3;
        uint32_t DELAY  : 4;
        uint32_t RSVD2  : 1;
        uint32_t DDS    : 1;
        uint32_t DMA    : 2;
        uint32_t ADCPRE : 2;   /* ADC prescaler: 00=/2 01=/4 10=/6 11=/8 */
        uint32_t RSVD3  : 4;
        uint32_t VBATE  : 1;
        uint32_t TSVREFE: 1;
        uint32_t RSVD4  : 8;
    } BITS;
} ADC_CCR_t;

#define ADC_CCR  ((volatile ADC_CCR_t *)ADC_CCR_BASE)

/* ============================================================
 *  SMPR Bit-field Helpers (3 bits per channel)
 * ============================================================ */
/* Sample time values (written into SMPRx per channel) */
#define ADC_SMP_3_CYCLES    0x0U
#define ADC_SMP_15_CYCLES   0x1U
#define ADC_SMP_28_CYCLES   0x2U
#define ADC_SMP_56_CYCLES   0x3U
#define ADC_SMP_84_CYCLES   0x4U
#define ADC_SMP_112_CYCLES  0x5U
#define ADC_SMP_144_CYCLES  0x6U
#define ADC_SMP_480_CYCLES  0x7U   /* slowest — best for high-impedance sensors */

#endif /* ADC_REGS_H_ */
