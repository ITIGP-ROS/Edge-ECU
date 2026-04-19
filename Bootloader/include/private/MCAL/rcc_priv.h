#ifndef RCC_PRIV_H
#define RCC_PRIV_H

#include "../../../lib/STD_Types.h"

#define RCC_BASE_ADDRESS  0x40023800UL

typedef struct {
    // Offset 0x00: RCC Clock Control Register (RCC_CR)
    union {
        volatile uint32_t REG;
        struct {
            volatile uint32_t HSION       : 1;
            volatile uint32_t HSIRDY      : 1;
            volatile uint32_t Reserved1   : 1;
            volatile uint32_t HSITRIM     : 5;
            volatile uint32_t HSICAL      : 8;
            volatile uint32_t HSEON       : 1;
            volatile uint32_t HSERDY      : 1;
            volatile uint32_t HSEBYP      : 1;
            volatile uint32_t CSSON       : 1;
            volatile uint32_t Reserved2   : 4;
            volatile uint32_t PLLON       : 1;
            volatile uint32_t PLLRDY      : 1;
            volatile uint32_t PLLI2SON    : 1;
            volatile uint32_t PLLI2SRDY   : 1;
            volatile uint32_t Reserved3   : 4;
        } BITS;
    } CR;

    // Offset 0x04: RCC PLL Configuration Register (RCC_PLLCFGR)
    union {
        volatile uint32_t REG;
        struct {
            volatile uint32_t PLLM        : 6;
            volatile uint32_t PLLN        : 9;
            volatile uint32_t Reserved1   : 1;
            volatile uint32_t PLLP        : 2;
            volatile uint32_t Reserved2   : 4;
            volatile uint32_t PLLSRC      : 1;
            volatile uint32_t Reserved3   : 1;
            volatile uint32_t PLLQ        : 4;
            volatile uint32_t Reserved4   : 4;
        } BITS;
    } PLLCFGR;

    // Offset 0x08: RCC Clock Configuration Register (RCC_CFGR)
    union {
        volatile uint32_t REG;
        struct {
            volatile uint32_t SW          : 2;
            volatile uint32_t SWS         : 2;
            volatile uint32_t HPRE        : 4;
            volatile uint32_t Reserved1   : 2;
            volatile uint32_t PPRE1       : 3;
            volatile uint32_t PPRE2       : 3;
            volatile uint32_t RTCPRE      : 5;
            volatile uint32_t MCO1        : 2;
            volatile uint32_t I2SSRC      : 1;
            volatile uint32_t MCO1PRE     : 3;
            volatile uint32_t MCO2PRE     : 3;
            volatile uint32_t MCO2        : 2;
        } BITS;
    } CFGR;

    // Offset 0x0C: RCC Clock Interrupt Register (RCC_CIR)
    union {
        volatile uint32_t REG;
        struct {
            volatile uint32_t LSIRDYF    : 1;
            volatile uint32_t LSERDYF    : 1;
            volatile uint32_t HSIRDYF    : 1;
            volatile uint32_t HSERDYF    : 1;
            volatile uint32_t PLLRDYF    : 1;
            volatile uint32_t PLLI2SRDYF : 1;
            volatile uint32_t Reserved1  : 1;
            volatile uint32_t CSSF       : 1;
            volatile uint32_t LSIRDYIE   : 1;
            volatile uint32_t LSERDYIE   : 1;
            volatile uint32_t HSIRDYIE   : 1;
            volatile uint32_t HSERDYIE   : 1;
            volatile uint32_t PLLRDYIE   : 1;
            volatile uint32_t PLLI2SRDYIE: 1;
            volatile uint32_t Reserved2  : 2;
            volatile uint32_t LSIRDYC    : 1;
            volatile uint32_t LSERDYC    : 1;
            volatile uint32_t HSIRDYC    : 1;
            volatile uint32_t HSERDYC    : 1;
            volatile uint32_t PLLRDYC    : 1;
            volatile uint32_t PLLI2SRDYC : 1;
            volatile uint32_t Reserved3  : 1;
            volatile uint32_t CSSC       : 1;
            volatile uint32_t Reserved4  : 8;
        } BITS;
    } CIR;

    // Offset 0x10: RCC AHB1 Peripheral Reset Register (RCC_AHB1RSTR)
    union {
        volatile uint32_t REG;
        struct {
            volatile uint32_t GPIOARST    : 1;
            volatile uint32_t GPIOBRST    : 1;
            volatile uint32_t GPIOCRST    : 1;
            volatile uint32_t GPIODRST    : 1;
            volatile uint32_t GPIOERST    : 1;
            volatile uint32_t Reserved1   : 2;
            volatile uint32_t GPIOHRST    : 1;
            volatile uint32_t Reserved2   : 4;
            volatile uint32_t CRCRST      : 1;
            volatile uint32_t Reserved3   : 8;
            volatile uint32_t DMA1RST     : 1;
            volatile uint32_t DMA2RST     : 1;
            volatile uint32_t Reserved4   : 9;
        } BITS;
    } AHB1RSTR;

    // Offset 0x14: RCC AHB2 Peripheral Reset Register (RCC_AHB2RSTR)
    union {
        volatile uint32_t REG;
        struct {
            volatile uint32_t Reserved1   : 7;
            volatile uint32_t OTGFSRST    : 1;
            volatile uint32_t Reserved2   : 24;
        } BITS;
    } AHB2RSTR;

    // Offset 0x18-0x1C: Reserved
    volatile uint32_t RESERVED0[2];

    // Offset 0x20: RCC APB1 Peripheral Reset Register (RCC_APB1RSTR)
    union {
        volatile uint32_t REG;
        struct {
            volatile uint32_t TIM2RST     : 1;
            volatile uint32_t TIM3RST     : 1;
            volatile uint32_t TIM4RST     : 1;
            volatile uint32_t TIM5RST     : 1;
            volatile uint32_t Reserved1   : 7;
            volatile uint32_t WWDGRST     : 1;
            volatile uint32_t Reserved2   : 2;
            volatile uint32_t SPI2RST     : 1;
            volatile uint32_t SPI3RST     : 1;
            volatile uint32_t Reserved3   : 1;
            volatile uint32_t USART2RST   : 1;
            volatile uint32_t Reserved4   : 3;
            volatile uint32_t I2C1RST     : 1;
            volatile uint32_t I2C2RST     : 1;
            volatile uint32_t I2C3RST     : 1;
            volatile uint32_t Reserved5   : 4;
            volatile uint32_t PWRRST      : 1;
            volatile uint32_t Reserved6   : 3;
        } BITS;
    } APB1RSTR;

    // Offset 0x24: RCC APB2 Peripheral Reset Register (RCC_APB2RSTR)
    union {
        volatile uint32_t REG;
        struct {
            volatile uint32_t TIM1RST     : 1;
            volatile uint32_t Reserved1   : 3;
            volatile uint32_t USART1RST   : 1;
            volatile uint32_t USART6RST   : 1;
            volatile uint32_t Reserved2   : 2;
            volatile uint32_t ADCRST      : 1;
            volatile uint32_t Reserved3   : 2;
            volatile uint32_t SDIORST     : 1;
            volatile uint32_t SPI1RST     : 1;
            volatile uint32_t SPI4RST     : 1;
            volatile uint32_t SYSCFGRST   : 1;
            volatile uint32_t Reserved5   : 1;
            volatile uint32_t TIM9RST     : 1;
            volatile uint32_t TIM10RST    : 1;
            volatile uint32_t TIM11RST    : 1;
            volatile uint32_t Reserved6   : 12;
        } BITS;
    } APB2RSTR;

    // Offset 0x28 - 0x2C: Reserved
    volatile uint32_t RESERVED1[2];

    // Offset 0x30: RCC AHB1 Peripheral Clock Enable Register (RCC_AHB1ENR)
    union {
        volatile uint32_t REG;
        struct {
            volatile uint32_t GPIOAEN     : 1;
            volatile uint32_t GPIOBEN     : 1;
            volatile uint32_t GPIOCEN     : 1;
            volatile uint32_t GPIODEN     : 1;
            volatile uint32_t GPIOEEN     : 1;
            volatile uint32_t Reserved1   : 2;
            volatile uint32_t GPIOHEN     : 1;
            volatile uint32_t Reserved2   : 4;
            volatile uint32_t CRCEN       : 1;
            volatile uint32_t Reserved3   : 8;
            volatile uint32_t DMA1EN      : 1;
            volatile uint32_t DMA2EN      : 1;
            volatile uint32_t Reserved4   : 8;
        } BITS;
    } AHB1ENR;

    // Offset 0x34: RCC AHB2 Peripheral Clock Enable Register (RCC_AHB2ENR)
    union {
        volatile uint32_t REG;
        struct {
            volatile uint32_t Reserved1   : 7;
            volatile uint32_t OTGFSEN     : 1;
            volatile uint32_t Reserved2   : 24;
        } BITS;
    } AHB2ENR;

    // Offset 0x38-0x3C: Reserved
    volatile uint32_t RESERVED2[2];

    // Offset 0x40: RCC APB1 Peripheral Clock Enable Register (RCC_APB1ENR)
    union {
        volatile uint32_t REG;
        struct {
            volatile uint32_t TIM2EN      : 1;
            volatile uint32_t TIM3EN      : 1;
            volatile uint32_t TIM4EN      : 1;
            volatile uint32_t TIM5EN      : 1;
            volatile uint32_t Reserved1   : 7;
            volatile uint32_t WWDGEN      : 1;
            volatile uint32_t Reserved2   : 2;
            volatile uint32_t SPI2EN      : 1;
            volatile uint32_t SPI3EN      : 1;
            volatile uint32_t Reserved3   : 1;
            volatile uint32_t USART2EN    : 1;
            volatile uint32_t Reserved4   : 3;
            volatile uint32_t I2C1EN      : 1;
            volatile uint32_t I2C2EN      : 1;
            volatile uint32_t I2C3EN      : 1;
            volatile uint32_t Reserved5   : 4;
            volatile uint32_t PWREN       : 1;
            volatile uint32_t Reserved6   : 3;
        } BITS;
    } APB1ENR;

    // Offset 0x44: RCC APB2 Peripheral Clock Enable Register (RCC_APB2ENR)
    union {
        volatile uint32_t REG;
        struct {
            volatile uint32_t TIM1EN      : 1;
            volatile uint32_t Reserved1   : 3;
            volatile uint32_t USART1EN    : 1;
            volatile uint32_t USART6EN    : 1;
            volatile uint32_t Reserved2   : 2;
            volatile uint32_t ADC1EN      : 1;
            volatile uint32_t Reserved3   : 2;
            volatile uint32_t SDIOEN      : 1;
            volatile uint32_t SPI1EN      : 1;
            volatile uint32_t SPI4EN      : 1;
            volatile uint32_t SYSCFGEN    : 1;
            volatile uint32_t Reserved5   : 1;
            volatile uint32_t TIM9EN      : 1;
            volatile uint32_t TIM10EN     : 1;
            volatile uint32_t TIM11EN     : 1;
            volatile uint32_t Reserved6   : 12;
        } BITS;
    } APB2ENR;

    // Offset 0x48 - 0x4C: Reserved
    volatile uint32_t RESERVED3[2];

    // Offset 0x50: RCC AHB1 Peripheral Low Power Clock Enable Register (RCC_AHB1LPENR)
    union {
        volatile uint32_t REG;
        struct {
            volatile uint32_t GPIOALPEN   : 1;
            volatile uint32_t GPIOBLPEN   : 1;
            volatile uint32_t GPIOCLPEN   : 1;
            volatile uint32_t GPIODLPEN   : 1;
            volatile uint32_t GPIOELPEN   : 1;
            volatile uint32_t Reserved1   : 2;
            volatile uint32_t GPIOHLPEN   : 1;
            volatile uint32_t Reserved2   : 4;
            volatile uint32_t CRCLPEN     : 1;
            volatile uint32_t Reserved3   : 2;
            volatile uint32_t FLITFLPEN   : 1;
            volatile uint32_t Reserved4   : 1;
            volatile uint32_t SRAM1LPEN   : 1;
            volatile uint32_t Reserved5   : 3;
            volatile uint32_t DMA1LPEN    : 1;
            volatile uint32_t DMA2LPEN    : 1;
            volatile uint32_t Reserved6   : 2;
            volatile uint32_t OTGFSLPEN   : 1;
            volatile uint32_t Reserved7   : 5;
        } BITS;
    } AHB1LPENR;

    // Offset 0x54: RCC AHB2 Peripheral Low Power Clock Enable Register (RCC_AHB2LPENR)
    union {
        volatile uint32_t REG;
        struct {
            volatile uint32_t Reserved1   : 7;
            volatile uint32_t OTGFSLPEN   : 1;
            volatile uint32_t Reserved2   : 24;
        } BITS;
    } AHB2LPENR;

    // Offset 0x58: RCC AHB3 Peripheral Low Power Clock Enable Register (RCC_AHB3LPENR)
    union {
        volatile uint32_t REG;
        struct {
            volatile uint32_t Reserved1   : 32; // STM32F401 has no peripherals on AHB3, this register is reserved
        } BITS;
    } AHB3LPENR;

    // Offset 0x5C: Reserved
    volatile uint32_t RESERVED4;

    // Offset 0x60: RCC APB1 Peripheral Low Power Clock Enable Register (RCC_APB1LPENR)
    union {
        volatile uint32_t REG;
        struct {
            volatile uint32_t TIM2LPEN    : 1;
            volatile uint32_t TIM3LPEN    : 1;
            volatile uint32_t TIM4LPEN    : 1;
            volatile uint32_t TIM5LPEN    : 1;
            volatile uint32_t Reserved1   : 7;
            volatile uint32_t WWDGLPEN    : 1;
            volatile uint32_t Reserved2   : 2;
            volatile uint32_t SPI2LPEN    : 1;
            volatile uint32_t SPI3LPEN    : 1;
            volatile uint32_t Reserved3   : 1;
            volatile uint32_t USART2LPEN  : 1;
            volatile uint32_t Reserved4   : 3;
            volatile uint32_t I2C1LPEN    : 1;
            volatile uint32_t I2C2LPEN    : 1;
            volatile uint32_t I2C3LPEN    : 1;
            volatile uint32_t Reserved5   : 4;
            volatile uint32_t PWRLPEN     : 1;
            volatile uint32_t Reserved6   : 3;
        } BITS;
    } APB1LPENR;

    // Offset 0x64: RCC APB2 Peripheral Low Power Clock Enable Register (RCC_APB2LPENR)
    union {
        volatile uint32_t REG;
        struct {
            volatile uint32_t TIM1LPEN    : 1;
            volatile uint32_t Reserved1   : 3;
            volatile uint32_t USART1LPEN  : 1;
            volatile uint32_t USART6LPEN  : 1;
            volatile uint32_t Reserved2   : 2;
            volatile uint32_t ADC1LPEN    : 1;
            volatile uint32_t Reserved3   : 2;
            volatile uint32_t SDIOLPEN    : 1;
            volatile uint32_t Reserved4   : 4;
            volatile uint32_t SPI1LPEN    : 1;
            volatile uint32_t SPI4LPEN    : 1;
            volatile uint32_t SYSCFGLPEN  : 1;
            volatile uint32_t Reserved5   : 1;
            volatile uint32_t TIM9LPEN    : 1;
            volatile uint32_t TIM10LPEN   : 1;
            volatile uint32_t TIM11LPEN   : 1;
            volatile uint32_t Reserved6   : 8;
        } BITS;
    } APB2LPENR;

    // Offset 0x68 - 0x6C: Reserved
    volatile uint32_t RESERVED5[2];

    // Offset 0x70: RCC Backup Domain Control Register (RCC_BDCR)
    union {
        volatile uint32_t REG;
        struct {
            volatile uint32_t LSEON       : 1;
            volatile uint32_t LSERDY      : 1;
            volatile uint32_t LSEBYP      : 1;
            volatile uint32_t Reserved1   : 5;
            volatile uint32_t RTCSEL      : 2;
            volatile uint32_t Reserved2   : 5;
            volatile uint32_t RTCEN       : 1;
            volatile uint32_t BDRST       : 1;
            volatile uint32_t Reserved3   : 15;
        } BITS;
    } BDCR;

    // Offset 0x74: RCC Clock Control and Status Register (RCC_CSR)
    union {
        volatile uint32_t REG;
        struct {
            volatile uint32_t LSION       : 1;
            volatile uint32_t LSIRDY      : 1;
            volatile uint32_t Reserved1   : 22;
            volatile uint32_t RMVF        : 1;
            volatile uint32_t Reserved2   : 1;
            volatile uint32_t BORRSTF     : 1;
            volatile uint32_t PADRSTF     : 1;
            volatile uint32_t PORRSTF     : 1;
            volatile uint32_t SFTRSTF     : 1;
            volatile uint32_t IWDGRSTF    : 1;
            volatile uint32_t WWDGRSTF    : 1;
            volatile uint32_t LPWRRSTF    : 1;
        } BITS;
    } CSR;

    // Offset 0x78 - 0x7C: Reserved
    volatile uint32_t RESERVED6[2];
    
    // Offset 0x80: RCC Spread Spectrum Clock Generation Register (RCC_SSCGR)
    // *Note: STM32F401 supports SSCG on the main PLL, if the chip version supports it.
    union {
        volatile uint32_t REG;
        struct {
            volatile uint32_t MODPER      : 13;
            volatile uint32_t INCSTEP     : 15;
            volatile uint32_t Reserved1   : 2;
            volatile uint32_t SPREADSEL   : 1;
            volatile uint32_t SSCGEN      : 1;
        } BITS;
    } SSCGR;

    // Offset 0x84: RCC PLLI2S Configuration Register (RCC_PLLI2SCFGR)
    union {
        volatile uint32_t REG;
        struct {
            volatile uint32_t Reserved1   : 6;
            volatile uint32_t PLLI2SN     : 9;
            volatile uint32_t Reserved2   : 5;
            volatile uint32_t PLLI2SR     : 3;
            volatile uint32_t Reserved3   : 9;
        } BITS;
    } PLLI2SCFGR;

    // Offset 0x88: Reserved (For PLLSAICFGR on other F4's, but not present on F401)
    volatile uint32_t RESERVED7;

    // Offset 0x8C: RCC Dedicated Clocks Configuration Register (RCC_DCKCFGR)
    union {
        volatile uint32_t REG;
        struct {
            volatile uint32_t Reserved1   : 28;
            volatile uint32_t PLLI2SDIV   : 2;
            volatile uint32_t Reserved2   : 2;
        } BITS;
    } DCKCFGR;

} RCC_t;

// RCC base pointer definition
#define RCC   ((RCC_t *) RCC_BASE_ADDRESS)

#endif // RCC_PRIV_H