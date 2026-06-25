#ifndef _RCC_REGS_H
#define _RCC_REGS_H

#include "STD_TYPES.h"

#define PLL_HSI_FREQ 16000000UL

/* Issue 1: PLL_HSE_FREQ is now compile-time overridable via build system.
   WARNING: This value MUST match the actual crystal frequency if HSE is used.
   Default is 25 MHz; override with -DPLL_HSE_FREQ=<freq> in your build system. */
#ifndef PLL_HSE_FREQ
#define PLL_HSE_FREQ 25000000UL
#endif

#define VCO_INPUT_MIN  1000000UL  /* 1 MHz */
#define VCO_INPUT_MAX  2000000UL  /* 2 MHz */
#define PLL_M_MIN      2U
#define PLL_M_MAX      63U
#define PLL_N_MIN      192U
#define PLL_N_MAX      432U
#define VCO_OUTPUT_MIN 192000000UL
#define VCO_OUTPUT_MAX 432000000UL
#define PLL_MAX_FREQ   84000000UL
#define PLL_USB_MAX_FREQ 48000000UL

#define RCC ((volatile RCC_REGS_t *)0x40023800)
#define CLK_NOT_READY 0U
#define RCC_CFGR_SW_HSI (0x00U)
#define RCC_CFGR_SW_HSE (0x01U)
#define RCC_CFGR_SW_PLL (0x02U)

#define RCC_CFGR_SWS_POS  (2U)
#define RCC_CFGR_SWS_MASK (0x03U << RCC_CFGR_SWS_POS)

/* Bit-extracting masks — Issue 12: added U suffix per MISRA-C:2012 Rule 7.2 */
#define BUS_MASK 0x60U  /* bits 6:5 — fixed: added U suffix (Issue 12) */
#define BIT_MASK 0x1FU  /* bits 4:0 — fixed: added U suffix (Issue 12) */

/* Issue 8: Named constants for APB bus markers, replacing magic numbers in GET_PCLK_FREQ */
#define PERIPH_BUS_APB1_MARKER 0x40U
#define PERIPH_BUS_APB2_MARKER 0x60U

/* Issue 6: NULL removed from this file — defined only in STD_TYPES.h to avoid duplication */

typedef enum {
    RCC_AHB1_BUS = 0U,
    RCC_AHB2_BUS,
    RCC_APB1_BUS,
    RCC_APB2_BUS
} RCC_BUS_t;

/* Issue 13: volatile removed from every individual bit-field member.
   volatile on the ALL member of each union is sufficient and correct per C99/C11.
   Per-field volatile is implementation-defined and non-standard. */
typedef struct {

    /* Offset 0x00: RCC Clock Control Register (RCC_CR) */
    union {
        volatile uint32_t ALL;
        struct {
            uint32_t HSION     : 1;
            uint32_t HSIRDY    : 1;
            uint32_t Reserved1 : 1;
            uint32_t HSITRIM   : 5;
            uint32_t HSICAL    : 8;
            uint32_t HSEON     : 1;
            uint32_t HSERDY    : 1;
            uint32_t HSEBYP    : 1;
            uint32_t CSSON     : 1;
            uint32_t Reserved2 : 4;
            uint32_t PLLON     : 1;
            uint32_t PLLRDY    : 1;
            uint32_t PLLI2SON  : 1;
            uint32_t PLLI2SRDY : 1;
            uint32_t Reserved3 : 4;
        } BITS;
    } CR;

    /* Offset 0x04: RCC PLL Configuration Register (RCC_PLLCFGR) */
    union {
        volatile uint32_t ALL;
        struct {
            uint32_t PLLM      : 6;
            uint32_t PLLN      : 9;
            uint32_t Reserved1 : 1;
            uint32_t PLLP      : 2;
            uint32_t Reserved2 : 4;
            uint32_t PLLSRC    : 1;
            uint32_t Reserved3 : 1;
            uint32_t PLLQ      : 4;
            uint32_t Reserved4 : 4;
        } BITS;
    } PLLCFGR;

    /* Offset 0x08: RCC Clock Configuration Register (RCC_CFGR) */
    union {
        volatile uint32_t ALL;
        struct {
            uint32_t SW        : 2;
            uint32_t SWS       : 2;
            uint32_t HPRE      : 4;
            uint32_t Reserved1 : 2;
            uint32_t PPRE1     : 3;
            uint32_t PPRE2     : 3;
            uint32_t RTCPRE    : 5;
            uint32_t MCO1      : 2;
            uint32_t I2SSRC    : 1;
            uint32_t MCO1PRE   : 3;
            uint32_t MCO2PRE   : 3;
            uint32_t MCO2      : 2;
        } BITS;
    } CFGR;

    /* Offset 0x0C: RCC Clock Interrupt Register (RCC_CIR) */
    union {
        volatile uint32_t ALL;
        struct {
            uint32_t LSIRDYF    : 1;
            uint32_t LSERDYF    : 1;
            uint32_t HSIRDYF    : 1;
            uint32_t HSERDYF    : 1;
            uint32_t PLLRDYF    : 1;
            uint32_t PLLI2SRDYF : 1;
            uint32_t Reserved1  : 1;
            uint32_t CSSF       : 1;
            uint32_t LSIRDYIE   : 1;
            uint32_t LSERDYIE   : 1;
            uint32_t HSIRDYIE   : 1;
            uint32_t HSERDYIE   : 1;
            uint32_t PLLRDYIE   : 1;
            uint32_t PLLI2SRDYIE: 1;
            uint32_t Reserved2  : 2;
            uint32_t LSIRDYC    : 1;
            uint32_t LSERDYC    : 1;
            uint32_t HSIRDYC    : 1;
            uint32_t HSERDYC    : 1;
            uint32_t PLLRDYC    : 1;
            uint32_t PLLI2SRDYC : 1;
            uint32_t Reserved3  : 1;
            uint32_t CSSC       : 1;
            uint32_t Reserved4  : 8;
        } BITS;
    } CIR;

    /* Offset 0x10: RCC AHB1 Peripheral Reset Register (RCC_AHB1RSTR) */
    union {
        volatile uint32_t ALL;
        struct {
            uint32_t GPIOARST  : 1;
            uint32_t GPIOBRST  : 1;
            uint32_t GPIOCRST  : 1;
            uint32_t GPIODRST  : 1;
            uint32_t GPIOERST  : 1;
            uint32_t Reserved1 : 2;
            uint32_t GPIOHRST  : 1;
            uint32_t Reserved2 : 4;
            uint32_t CRCRST    : 1;
            uint32_t Reserved3 : 8;
            uint32_t DMA1RST   : 1;
            uint32_t DMA2RST   : 1;
            uint32_t Reserved4 : 9;
        } BITS;
    } AHB1RSTR;

    /* Offset 0x14: RCC AHB2 Peripheral Reset Register (RCC_AHB2RSTR) */
    union {
        volatile uint32_t ALL;
        struct {
            uint32_t Reserved1 : 7;
            uint32_t OTGFSRST  : 1;
            uint32_t Reserved2 : 24;
        } BITS;
    } AHB2RSTR;

    /* Offset 0x18-0x1C: Reserved */
    volatile uint32_t RESERVED0[2];

    /* Offset 0x20: RCC APB1 Peripheral Reset Register (RCC_APB1RSTR) */
    union {
        volatile uint32_t ALL;
        struct {
            uint32_t TIM2RST   : 1;
            uint32_t TIM3RST   : 1;
            uint32_t TIM4RST   : 1;
            uint32_t TIM5RST   : 1;
            uint32_t Reserved1 : 7;
            uint32_t WWDGRST   : 1;
            uint32_t Reserved2 : 2;
            uint32_t SPI2RST   : 1;
            uint32_t SPI3RST   : 1;
            uint32_t Reserved3 : 1;
            uint32_t USART2RST : 1;
            uint32_t Reserved4 : 3;
            uint32_t I2C1RST   : 1;
            uint32_t I2C2RST   : 1;
            uint32_t I2C3RST   : 1;
            uint32_t Reserved5 : 4;
            uint32_t PWRRST    : 1;
            uint32_t Reserved6 : 3;
        } BITS;
    } APB1RSTR;

    /* Offset 0x24: RCC APB2 Peripheral Reset Register (RCC_APB2RSTR) */
    union {
        volatile uint32_t ALL;
        struct {
            uint32_t TIM1RST   : 1;
            uint32_t Reserved1 : 3;
            uint32_t USART1RST : 1;
            uint32_t USART6RST : 1;
            uint32_t Reserved2 : 2;
            uint32_t ADCRST    : 1;
            uint32_t Reserved3 : 2;
            uint32_t SDIORST   : 1;
            uint32_t SPI1RST   : 1;
            uint32_t SPI4RST   : 1;
            uint32_t SYSCFGRST : 1;
            uint32_t Reserved5 : 1;
            uint32_t TIM9RST   : 1;
            uint32_t TIM10RST  : 1;
            uint32_t TIM11RST  : 1;
            uint32_t Reserved6 : 12;
        } BITS;
    } APB2RSTR;

    /* Offset 0x28 - 0x2C: Reserved */
    volatile uint32_t RESERVED1[2];

    /* Offset 0x30: RCC AHB1 Peripheral Clock Enable Register (RCC_AHB1ENR) */
    union {
        volatile uint32_t ALL;
        struct {
            uint32_t GPIOAEN   : 1;
            uint32_t GPIOBEN   : 1;
            uint32_t GPIOCEN   : 1;
            uint32_t GPIODEN   : 1;
            uint32_t GPIOEEN   : 1;
            uint32_t Reserved1 : 2;
            uint32_t GPIOHEN   : 1;
            uint32_t Reserved2 : 4;
            uint32_t CRCEN     : 1;
            uint32_t Reserved3 : 8;
            uint32_t DMA1EN    : 1;
            uint32_t DMA2EN    : 1;
            uint32_t Reserved4 : 8;
        } BITS;
    } AHB1ENR;

    /* Offset 0x34: RCC AHB2 Peripheral Clock Enable Register (RCC_AHB2ENR) */
    union {
        volatile uint32_t ALL;
        struct {
            uint32_t Reserved1 : 7;
            uint32_t OTGFSEN   : 1;
            uint32_t Reserved2 : 24;
        } BITS;
    } AHB2ENR;

    /* Offset 0x38-0x3C: Reserved */
    volatile uint32_t RESERVED2[2];

    /* Offset 0x40: RCC APB1 Peripheral Clock Enable Register (RCC_APB1ENR) */
    union {
        volatile uint32_t ALL;
        struct {
            uint32_t TIM2EN    : 1;
            uint32_t TIM3EN    : 1;
            uint32_t TIM4EN    : 1;
            uint32_t TIM5EN    : 1;
            uint32_t Reserved1 : 7;
            uint32_t WWDGEN    : 1;
            uint32_t Reserved2 : 2;
            uint32_t SPI2EN    : 1;
            uint32_t SPI3EN    : 1;
            uint32_t Reserved3 : 1;
            uint32_t USART2EN  : 1;
            uint32_t Reserved4 : 3;
            uint32_t I2C1EN    : 1;
            uint32_t I2C2EN    : 1;
            uint32_t I2C3EN    : 1;
            uint32_t Reserved5 : 4;
            uint32_t PWREN     : 1;
            uint32_t Reserved6 : 3;
        } BITS;
    } APB1ENR;

    /* Offset 0x44: RCC APB2 Peripheral Clock Enable Register (RCC_APB2ENR) */
    union {
        volatile uint32_t ALL;
        struct {
            uint32_t TIM1EN    : 1;
            uint32_t Reserved1 : 3;
            uint32_t USART1EN  : 1;
            uint32_t USART6EN  : 1;
            uint32_t Reserved2 : 2;
            uint32_t ADC1EN    : 1;
            uint32_t Reserved3 : 2;
            uint32_t SDIOEN    : 1;
            uint32_t SPI1EN    : 1;
            uint32_t SPI4EN    : 1;
            uint32_t SYSCFGEN  : 1;
            uint32_t Reserved5 : 1;
            uint32_t TIM9EN    : 1;
            uint32_t TIM10EN   : 1;
            uint32_t TIM11EN   : 1;
            uint32_t Reserved6 : 12;
        } BITS;
    } APB2ENR;

    /* Offset 0x48 - 0x4C: Reserved */
    volatile uint32_t RESERVED3[2];

    /* Offset 0x50: RCC AHB1 Peripheral Low Power Clock Enable Register (RCC_AHB1LPENR) */
    union {
        volatile uint32_t ALL;
        struct {
            uint32_t GPIOALPEN  : 1;
            uint32_t GPIOBLPEN  : 1;
            uint32_t GPIOCLPEN  : 1;
            uint32_t GPIODLPEN  : 1;
            uint32_t GPIOELPEN  : 1;
            uint32_t Reserved1  : 2;
            uint32_t GPIOHLPEN  : 1;
            uint32_t Reserved2  : 4;
            uint32_t CRCLPEN    : 1;
            uint32_t Reserved3  : 2;
            uint32_t FLITFLPEN  : 1;
            uint32_t Reserved4  : 1;
            uint32_t SRAM1LPEN  : 1;
            uint32_t Reserved5  : 3;
            uint32_t DMA1LPEN   : 1;
            uint32_t DMA2LPEN   : 1;
            uint32_t Reserved6  : 2;
            uint32_t OTGFSLPEN  : 1;
            uint32_t Reserved7  : 5;
        } BITS;
    } AHB1LPENR;

    /* Offset 0x54: RCC AHB2 Peripheral Low Power Clock Enable Register (RCC_AHB2LPENR) */
    union {
        volatile uint32_t ALL;
        struct {
            uint32_t Reserved1  : 7;
            uint32_t OTGFSLPEN  : 1;
            uint32_t Reserved2  : 24;
        } BITS;
    } AHB2LPENR;

    /* Offset 0x58: RCC AHB3 Peripheral Low Power Clock Enable Register (RCC_AHB3LPENR) */
    union {
        volatile uint32_t ALL;
        struct {
            uint32_t Reserved1  : 32;  /* STM32F401 has no peripherals on AHB3 */
        } BITS;
    } AHB3LPENR;

    /* Offset 0x5C: Reserved */
    volatile uint32_t RESERVED4;

    /* Offset 0x60: RCC APB1 Peripheral Low Power Clock Enable Register (RCC_APB1LPENR) */
    union {
        volatile uint32_t ALL;
        struct {
            uint32_t TIM2LPEN   : 1;
            uint32_t TIM3LPEN   : 1;
            uint32_t TIM4LPEN   : 1;
            uint32_t TIM5LPEN   : 1;
            uint32_t Reserved1  : 7;
            uint32_t WWDGLPEN   : 1;
            uint32_t Reserved2  : 2;
            uint32_t SPI2LPEN   : 1;
            uint32_t SPI3LPEN   : 1;
            uint32_t Reserved3  : 1;
            uint32_t USART2LPEN : 1;
            uint32_t Reserved4  : 3;
            uint32_t I2C1LPEN   : 1;
            uint32_t I2C2LPEN   : 1;
            uint32_t I2C3LPEN   : 1;
            uint32_t Reserved5  : 4;
            uint32_t PWRLPEN    : 1;
            uint32_t Reserved6  : 3;
        } BITS;
    } APB1LPENR;

    /* Offset 0x64: RCC APB2 Peripheral Low Power Clock Enable Register (RCC_APB2LPENR) */
    union {
        volatile uint32_t ALL;
        struct {
            uint32_t TIM1LPEN   : 1;
            uint32_t Reserved1  : 3;
            uint32_t USART1LPEN : 1;
            uint32_t USART6LPEN : 1;
            uint32_t Reserved2  : 2;
            uint32_t ADC1LPEN   : 1;
            uint32_t Reserved3  : 2;
            uint32_t SDIOLPEN   : 1;
            uint32_t Reserved4  : 4;
            uint32_t SPI1LPEN   : 1;
            uint32_t SPI4LPEN   : 1;
            uint32_t SYSCFGLPEN : 1;
            uint32_t Reserved5  : 1;
            uint32_t TIM9LPEN   : 1;
            uint32_t TIM10LPEN  : 1;
            uint32_t TIM11LPEN  : 1;
            uint32_t Reserved6  : 8;
        } BITS;
    } APB2LPENR;

    /* Offset 0x68 - 0x6C: Reserved */
    volatile uint32_t RESERVED5[2];

    /* Offset 0x70: RCC Backup Domain Control Register (RCC_BDCR) */
    union {
        volatile uint32_t ALL;
        struct {
            uint32_t LSEON     : 1;
            uint32_t LSERDY    : 1;
            uint32_t LSEBYP    : 1;
            uint32_t Reserved1 : 5;
            uint32_t RTCSEL    : 2;
            uint32_t Reserved2 : 5;
            uint32_t RTCEN     : 1;
            uint32_t BDRST     : 1;
            uint32_t Reserved3 : 15;
        } BITS;
    } BDCR;

    /* Offset 0x74: RCC Clock Control and Status Register (RCC_CSR) */
    union {
        volatile uint32_t ALL;
        struct {
            uint32_t LSION     : 1;
            uint32_t LSIRDY    : 1;
            uint32_t Reserved1 : 22;
            uint32_t RMVF      : 1;
            uint32_t Reserved2 : 1;
            uint32_t BORRSTF   : 1;
            uint32_t PADRSTF   : 1;
            uint32_t PORRSTF   : 1;
            uint32_t SFTRSTF   : 1;
            uint32_t IWDGRSTF  : 1;
            uint32_t WWDGRSTF  : 1;
            uint32_t LPWRRSTF  : 1;
        } BITS;
    } CSR;

    /* Offset 0x78 - 0x7C: Reserved */
    volatile uint32_t RESERVED6[2];

    /* Offset 0x80: RCC Spread Spectrum Clock Generation Register (RCC_SSCGR) */
    union {
        volatile uint32_t ALL;
        struct {
            uint32_t MODPER    : 13;
            uint32_t INCSTEP   : 15;
            uint32_t Reserved1 : 2;
            uint32_t SPREADSEL : 1;
            uint32_t SSCGEN    : 1;
        } BITS;
    } SSCGR;

    /* Offset 0x84: RCC PLLI2S Configuration Register (RCC_PLLI2SCFGR) */
    union {
        volatile uint32_t ALL;
        struct {
            uint32_t Reserved1  : 6;
            uint32_t PLLI2SN    : 9;
            uint32_t Reserved2  : 5;
            uint32_t PLLI2SR    : 3;
            uint32_t Reserved3  : 9;
        } BITS;
    } PLLI2SCFGR;

    /* Offset 0x88: Reserved (PLLSAICFGR on other F4s, not present on F401) */
    volatile uint32_t RESERVED7;

    /* Offset 0x8C: RCC Dedicated Clocks Configuration Register (RCC_DCKCFGR) */
    union {
        volatile uint32_t ALL;
        struct {
            uint32_t Reserved1  : 28;
            uint32_t PLLI2SDIV  : 2;
            uint32_t Reserved2  : 2;
        } BITS;
    } DCKCFGR;

} RCC_REGS_t;

#endif
