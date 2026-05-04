#ifndef GPIO_REGS_H_
#define GPIO_REGS_H_
#include "STD_TYPES.h"

#define GPIOA_BASE_ADDR 0x40020000U
#define GPIOB_BASE_ADDR 0x40020400U
#define GPIOC_BASE_ADDR 0x40020800U
#define GPIOD_BASE_ADDR 0x40020C00U
#define GPIOE_BASE_ADDR 0x40021000U
#define GPIOH_BASE_ADDR 0x40021C00U

#define GPIOA ((volatile GPIO_REGS_t *)GPIOA_BASE_ADDR)
#define GPIOB ((volatile GPIO_REGS_t *)GPIOB_BASE_ADDR)
#define GPIOC ((volatile GPIO_REGS_t *)GPIOC_BASE_ADDR)
#define GPIOD ((volatile GPIO_REGS_t *)GPIOD_BASE_ADDR)
#define GPIOE ((volatile GPIO_REGS_t *)GPIOE_BASE_ADDR)
#define GPIOH ((volatile GPIO_REGS_t *)GPIOH_BASE_ADDR)

#define GPIO_PIN_MAX_INDEX     15U  /* G-14: renamed from MAX_GPIO_PINS; this is the max valid pin INDEX (0..15), not the count */
#define MAX_GPIO_MODES         3U
#define MAX_GPIO_AF          15U


/* G-02: fixed swapped values — PIN0=bit0, PIN1=bit1, PIN2=bit2 (were shuffled) */
#define GPIO_PIN0_MASK         0x0001U  /* G-02: was 0x0004U (wrong) */
#define GPIO_PIN1_MASK         0x0002U  /* G-02: was 0x0001U (wrong) */
#define GPIO_PIN2_MASK         0x0004U  /* G-02: was 0x0002U (wrong) */
#define GPIO_PIN3_MASK         0x0008U
#define GPIO_PIN4_MASK         0x0010U
#define GPIO_PIN5_MASK         0x0020U
#define GPIO_PIN6_MASK         0x0040U
#define GPIO_PIN7_MASK         0x0080U
#define GPIO_PIN8_MASK         0x0100U
#define GPIO_PIN9_MASK         0x0200U
#define GPIO_PIN10_MASK        0x0400U
#define GPIO_PIN11_MASK        0x0800U
#define GPIO_PIN12_MASK        0x1000U
#define GPIO_PIN13_MASK        0x2000U
#define GPIO_PIN14_MASK        0x4000U
#define GPIO_PIN15_MASK        0x8000U
#define GPIO_ALL_PINS_MASK     0xFFFFU

/* G-06: Static per-pin mode masks — not used by driver internals which use GPIO_MODE_MASK(pin) macro.
         Retained for external caller convenience. */
#define GPIO_PIN0_MODE_MASK   0x00000003U
#define GPIO_PIN1_MODE_MASK   0x0000000CU
#define GPIO_PIN2_MODE_MASK   0x00000030U
#define GPIO_PIN3_MODE_MASK   0x000000C0U
#define GPIO_PIN4_MODE_MASK   0x00000300U
#define GPIO_PIN5_MODE_MASK   0x00000C00U
#define GPIO_PIN6_MODE_MASK   0x00003000U
#define GPIO_PIN7_MODE_MASK   0x0000C000U
#define GPIO_PIN8_MODE_MASK   0x00030000U
#define GPIO_PIN9_MODE_MASK   0x000C0000U
#define GPIO_PIN10_MODE_MASK  0x00300000U
#define GPIO_PIN11_MODE_MASK  0x00C00000U
#define GPIO_PIN12_MODE_MASK  0x03000000U
#define GPIO_PIN13_MODE_MASK  0x0C000000U
#define GPIO_PIN14_MODE_MASK  0x30000000U
#define GPIO_PIN15_MODE_MASK  0xC0000000U


#define GPIO_MODE_MASK(pin)        (0x03U << ((pin) * 2))
#define GPIO_OTYPE_MASK(pin)       (0x01U << (pin))
/* G-03: Intentionally identical to GPIO_MODE_MASK — MODER and OSPEEDR both use 2 bits per pin at the same bit positions */
#define GPIO_OSPEED_MASK(pin)      (0x03U << ((pin) * 2))
#define GPIO_PUPDR_MASK(pin)       (0x03U << ((pin) * 2))
#define GPIO_AFRL_MASK(pin)        (0x0FU << ((pin) * 4))
/* G-04: Valid only for pin 8–15; caller is responsible for enforcing this — do NOT call with pin < 8 */
#define GPIO_AFRH_MASK(pin)        (0x0FU << (((pin) - 8U) * 4U))
#define GPIO_BSRR_SET_MASK(pin)    (0x01U << (pin))
#define GPIO_BSRR_RESET_MASK(pin)  (0x01U << ((pin) + 16U))
#define GPIO_LCKR_MASK(pin)        (0x01U << (pin))
#define GPIO_ODR_MASK(pin)         (0x01U << (pin))

/* G-01: Removed redefinition of NULL — it is already defined in STD_TYPES.h */

typedef struct
{
    /* ======================== MODER ======================== */
    union {
        volatile uint32_t ALL;
        struct {
            /* G-05: removed volatile from individual bit-field members; volatile belongs only on ALL */
            uint32_t MODER0  : 2;
            uint32_t MODER1  : 2;
            uint32_t MODER2  : 2;
            uint32_t MODER3  : 2;
            uint32_t MODER4  : 2;
            uint32_t MODER5  : 2;
            uint32_t MODER6  : 2;
            uint32_t MODER7  : 2;
            uint32_t MODER8  : 2;
            uint32_t MODER9  : 2;
            uint32_t MODER10 : 2;
            uint32_t MODER11 : 2;
            uint32_t MODER12 : 2;
            uint32_t MODER13 : 2;
            uint32_t MODER14 : 2;
            uint32_t MODER15 : 2;
        } BITS;
    } MODER;

    /* ======================== OTYPER ======================== */
    union {
        volatile uint32_t ALL;
        struct {
            /* G-05: removed volatile from individual bit-field members */
            uint32_t OT0  : 1;
            uint32_t OT1  : 1;
            uint32_t OT2  : 1;
            uint32_t OT3  : 1;
            uint32_t OT4  : 1;
            uint32_t OT5  : 1;
            uint32_t OT6  : 1;
            uint32_t OT7  : 1;
            uint32_t OT8  : 1;
            uint32_t OT9  : 1;
            uint32_t OT10 : 1;
            uint32_t OT11 : 1;
            uint32_t OT12 : 1;
            uint32_t OT13 : 1;
            uint32_t OT14 : 1;
            uint32_t OT15 : 1;
            uint32_t RESERVED : 16;
        } BITS;
    } OTYPER;

    /* ======================== OSPEEDR ======================== */
    union {
        volatile uint32_t ALL;
        struct {
            /* G-05: removed volatile from individual bit-field members */
            uint32_t OSPEEDR0  : 2;
            uint32_t OSPEEDR1  : 2;
            uint32_t OSPEEDR2  : 2;
            uint32_t OSPEEDR3  : 2;
            uint32_t OSPEEDR4  : 2;
            uint32_t OSPEEDR5  : 2;
            uint32_t OSPEEDR6  : 2;
            uint32_t OSPEEDR7  : 2;
            uint32_t OSPEEDR8  : 2;
            uint32_t OSPEEDR9  : 2;
            uint32_t OSPEEDR10 : 2;
            uint32_t OSPEEDR11 : 2;
            uint32_t OSPEEDR12 : 2;
            uint32_t OSPEEDR13 : 2;
            uint32_t OSPEEDR14 : 2;
            uint32_t OSPEEDR15 : 2;
        } BITS;
    } OSPEEDR;

    /* ======================== PUPDR ======================== */
    union {
        volatile uint32_t ALL;
        struct {
            /* G-05: removed volatile from individual bit-field members */
            uint32_t PUPDR0  : 2;
            uint32_t PUPDR1  : 2;
            uint32_t PUPDR2  : 2;
            uint32_t PUPDR3  : 2;
            uint32_t PUPDR4  : 2;
            uint32_t PUPDR5  : 2;
            uint32_t PUPDR6  : 2;
            uint32_t PUPDR7  : 2;
            uint32_t PUPDR8  : 2;
            uint32_t PUPDR9  : 2;
            uint32_t PUPDR10 : 2;
            uint32_t PUPDR11 : 2;
            uint32_t PUPDR12 : 2;
            uint32_t PUPDR13 : 2;
            uint32_t PUPDR14 : 2;
            uint32_t PUPDR15 : 2;
        } BITS;
    } PUPDR;

    /* ======================== IDR ======================== */
    union {
        volatile uint32_t ALL;
        struct {
            /* G-05: removed volatile from individual bit-field members */
            uint32_t IDR0  : 1;
            uint32_t IDR1  : 1;
            uint32_t IDR2  : 1;
            uint32_t IDR3  : 1;
            uint32_t IDR4  : 1;
            uint32_t IDR5  : 1;
            uint32_t IDR6  : 1;
            uint32_t IDR7  : 1;
            uint32_t IDR8  : 1;
            uint32_t IDR9  : 1;
            uint32_t IDR10 : 1;
            uint32_t IDR11 : 1;
            uint32_t IDR12 : 1;
            uint32_t IDR13 : 1;
            uint32_t IDR14 : 1;
            uint32_t IDR15 : 1;
            uint32_t RESERVED : 16;
        } BITS;
    } IDR;

    /* ======================== ODR ======================== */
    union {
        volatile uint32_t ALL;
        struct {
            /* G-05: removed volatile from individual bit-field members */
            uint32_t ODR0  : 1;
            uint32_t ODR1  : 1;
            uint32_t ODR2  : 1;
            uint32_t ODR3  : 1;
            uint32_t ODR4  : 1;
            uint32_t ODR5  : 1;
            uint32_t ODR6  : 1;
            uint32_t ODR7  : 1;
            uint32_t ODR8  : 1;
            uint32_t ODR9  : 1;
            uint32_t ODR10 : 1;
            uint32_t ODR11 : 1;
            uint32_t ODR12 : 1;
            uint32_t ODR13 : 1;
            uint32_t ODR14 : 1;
            uint32_t ODR15 : 1;
            uint32_t RESERVED : 16;
        } BITS;
    } ODR;

    /* ======================== BSRR ======================== */
    union {
        volatile uint32_t ALL;
        struct {
            /* G-05: removed volatile from individual bit-field members */
            uint32_t BS0  : 1;
            uint32_t BS1  : 1;
            uint32_t BS2  : 1;
            uint32_t BS3  : 1;
            uint32_t BS4  : 1;
            uint32_t BS5  : 1;
            uint32_t BS6  : 1;
            uint32_t BS7  : 1;
            uint32_t BS8  : 1;
            uint32_t BS9  : 1;
            uint32_t BS10 : 1;
            uint32_t BS11 : 1;
            uint32_t BS12 : 1;
            uint32_t BS13 : 1;
            uint32_t BS14 : 1;
            uint32_t BS15 : 1;

            uint32_t BR0  : 1;
            uint32_t BR1  : 1;
            uint32_t BR2  : 1;
            uint32_t BR3  : 1;
            uint32_t BR4  : 1;
            uint32_t BR5  : 1;
            uint32_t BR6  : 1;
            uint32_t BR7  : 1;
            uint32_t BR8  : 1;
            uint32_t BR9  : 1;
            uint32_t BR10 : 1;
            uint32_t BR11 : 1;
            uint32_t BR12 : 1;
            uint32_t BR13 : 1;
            uint32_t BR14 : 1;
            uint32_t BR15 : 1;
        } BITS;
    } BSRR;

    /* ======================== LCKR ======================== */
    union {
        volatile uint32_t ALL;
        struct {
            /* G-05: removed volatile from individual bit-field members */
            uint32_t LCK0  : 1;
            uint32_t LCK1  : 1;
            uint32_t LCK2  : 1;
            uint32_t LCK3  : 1;
            uint32_t LCK4  : 1;
            uint32_t LCK5  : 1;
            uint32_t LCK6  : 1;
            uint32_t LCK7  : 1;
            uint32_t LCK8  : 1;
            uint32_t LCK9  : 1;
            uint32_t LCK10 : 1;
            uint32_t LCK11 : 1;
            uint32_t LCK12 : 1;
            uint32_t LCK13 : 1;
            uint32_t LCK14 : 1;
            uint32_t LCK15 : 1;
            uint32_t LCKK  : 1;
            uint32_t RESERVED : 15;
        } BITS;
    } LCKR;

    /* ======================== AFRL ======================== */
    union {
        volatile uint32_t ALL;
        struct {
            /* G-05: removed volatile from individual bit-field members */
            uint32_t AFR0 : 4;
            uint32_t AFR1 : 4;
            uint32_t AFR2 : 4;
            uint32_t AFR3 : 4;
            uint32_t AFR4 : 4;
            uint32_t AFR5 : 4;
            uint32_t AFR6 : 4;
            uint32_t AFR7 : 4;
        } BITS;
    } AFRL;

    /* ======================== AFRH ======================== */
    union {
        volatile uint32_t ALL;
        struct {
            /* G-05: removed volatile from individual bit-field members */
            uint32_t AFR8  : 4;
            uint32_t AFR9  : 4;
            uint32_t AFR10 : 4;
            uint32_t AFR11 : 4;
            uint32_t AFR12 : 4;
            uint32_t AFR13 : 4;
            uint32_t AFR14 : 4;
            uint32_t AFR15 : 4;
        } BITS;
    } AFRH;

} GPIO_REGS_t;




 /* ===== Alternate function macros  ===== */
#define AF0_SYSTEM      0x00U
#define AF1_TIM_1_2     0x01U
#define AF2_TIM_3_5     0x02U
#define AF3_TIM_9_11    0x03U
#define AF4_I2C_1_3     0x04U
#define AF5_SPI_1_4     0x05U
#define AF6_SPI_3       0x06U
#define AF7_USART_1_2   0x07U
#define AF8_USART_6     0x08U
#define AF9_I2C_2_3     0x09U
#define AF10_OTG_FS     0x0AU
#define AF12_SDIO       0x0CU
#define AF15_EVENTOUT   0x0FU


/* ======================= PORT A ======================= */
// PA0
#define PA0_AF_SYSTEM            AF0_SYSTEM
#define PA0_AF_TIM2_CH1_ETR      AF1_TIM_1_2
#define PA0_AF_TIM5_CH1          AF2_TIM_3_5
#define PA0_AF_USART2_CTS        AF7_USART_1_2
#define PA0_AF_EVENTOUT          AF15_EVENTOUT

// PA1
#define PA1_AF_SYSTEM            AF0_SYSTEM
#define PA1_AF_TIM2_CH2          AF1_TIM_1_2
#define PA1_AF_TIM5_CH2          AF2_TIM_3_5
#define PA1_AF_USART2_RTS        AF7_USART_1_2
#define PA1_AF_EVENTOUT          AF15_EVENTOUT

// PA2
#define PA2_AF_SYSTEM            AF0_SYSTEM
#define PA2_AF_TIM2_CH3          AF1_TIM_1_2
#define PA2_AF_TIM5_CH3          AF2_TIM_3_5
#define PA2_AF_TIM9_CH1          AF3_TIM_9_11
#define PA2_AF_USART2_TX         AF7_USART_1_2
#define PA2_AF_EVENTOUT          AF15_EVENTOUT

// PA3
#define PA3_AF_SYSTEM            AF0_SYSTEM
#define PA3_AF_TIM2_CH4          AF1_TIM_1_2
#define PA3_AF_TIM5_CH4          AF2_TIM_3_5
#define PA3_AF_TIM9_CH2          AF3_TIM_9_11
#define PA3_AF_USART2_RX         AF7_USART_1_2
#define PA3_AF_EVENTOUT          AF15_EVENTOUT

// PA4
#define PA4_AF_SYSTEM            AF0_SYSTEM
#define PA4_AF_SPI1_NSS          AF5_SPI_1_4
#define PA4_AF_SPI3_NSS_I2S3_WS  AF6_SPI_3
#define PA4_AF_USART2_CK         AF7_USART_1_2
#define PA4_AF_EVENTOUT          AF15_EVENTOUT

// PA5
#define PA5_AF_SYSTEM            AF0_SYSTEM
#define PA5_AF_SPI1_SCK          AF5_SPI_1_4
#define PA5_AF_TIM2_CH1_ETR      AF1_TIM_1_2
#define PA5_AF_EVENTOUT          AF15_EVENTOUT

// PA6
#define PA6_AF_SYSTEM            AF0_SYSTEM
#define PA6_AF_SPI1_MISO         AF5_SPI_1_4
#define PA6_AF_TIM3_CH1          AF2_TIM_3_5
#define PA6_AF_EVENTOUT          AF15_EVENTOUT

// PA7
#define PA7_AF_SYSTEM            AF0_SYSTEM
#define PA7_AF_SPI1_MOSI         AF5_SPI_1_4
#define PA7_AF_TIM3_CH2          AF2_TIM_3_5
#define PA7_AF_EVENTOUT          AF15_EVENTOUT

// PA8
#define PA8_AF_SYSTEM            AF0_SYSTEM   /* MCO1, JTMS not here; also I2C3_SCL */
#define PA8_AF_TIM1_CH1          AF1_TIM_1_2
#define PA8_AF_I2C3_SCL          AF4_I2C_1_3
#define PA8_AF_USART1_CK         AF7_USART_1_2
#define PA8_AF_EVENTOUT          AF15_EVENTOUT

// PA9
#define PA9_AF_SYSTEM            AF0_SYSTEM   /* VBUS is an input; not AF */
#define PA9_AF_TIM1_CH2          AF1_TIM_1_2
#define PA9_AF_USART1_TX         AF7_USART_1_2
#define PA9_AF_EVENTOUT          AF15_EVENTOUT

// PA10
#define PA10_AF_SYSTEM           AF0_SYSTEM
#define PA10_AF_TIM1_CH3         AF1_TIM_1_2
#define PA10_AF_USART1_RX        AF7_USART_1_2
#define PA10_AF_OTG_FS_ID        AF10_OTG_FS
#define PA10_AF_EVENTOUT         AF15_EVENTOUT

// PA11
#define PA11_AF_SYSTEM           AF0_SYSTEM
#define PA11_AF_TIM1_CH4         AF1_TIM_1_2
#define PA11_AF_OTG_FS_DM        AF10_OTG_FS
#define PA11_AF_EVENTOUT         AF15_EVENTOUT

// PA12
#define PA12_AF_SYSTEM           AF0_SYSTEM
#define PA12_AF_TIM1_ETR         AF1_TIM_1_2
#define PA12_AF_OTG_FS_DP        AF10_OTG_FS
#define PA12_AF_EVENTOUT         AF15_EVENTOUT

// PA13
#define PA13_AF_SYSTEM           AF0_SYSTEM   /* JTMS/SWDIO */
#define PA13_AF_EVENTOUT         AF15_EVENTOUT

// PA14
#define PA14_AF_SYSTEM           AF0_SYSTEM   /* JTCK/SWCLK */
#define PA14_AF_EVENTOUT         AF15_EVENTOUT

// PA15
#define PA15_AF_SYSTEM           AF0_SYSTEM   /* JTDI */
#define PA15_AF_TIM2_CH1_ETR     AF1_TIM_1_2
#define PA15_AF_SPI1_NSS         AF5_SPI_1_4
#define PA15_AF_SPI3_NSS         AF6_SPI_3
#define PA15_AF_EVENTOUT         AF15_EVENTOUT

/* ======================= PORT B ======================= */
// PB0
#define PB0_AF_SYSTEM            AF0_SYSTEM
#define PB0_AF_TIM1_CH2N         AF1_TIM_1_2
#define PB0_AF_TIM3_CH3          AF2_TIM_3_5
#define PB0_AF_EVENTOUT          AF15_EVENTOUT

// PB1
#define PB1_AF_SYSTEM            AF0_SYSTEM
#define PB1_AF_TIM1_CH3N         AF1_TIM_1_2
#define PB1_AF_TIM3_CH4          AF2_TIM_3_5
#define PB1_AF_EVENTOUT          AF15_EVENTOUT

// PB2
#define PB2_AF_SYSTEM            AF0_SYSTEM   /* BOOT1 on this package */
#define PB2_AF_EVENTOUT          AF15_EVENTOUT

// PB3
#define PB3_AF_SYSTEM            AF0_SYSTEM   /* JTDO/TRACESWO */
#define PB3_AF_TIM2_CH2          AF1_TIM_1_2
#define PB3_AF_SPI1_SCK          AF5_SPI_1_4
#define PB3_AF_SPI3_SCK          AF6_SPI_3
#define PB3_AF_EVENTOUT          AF15_EVENTOUT

// PB4
#define PB4_AF_SYSTEM            AF0_SYSTEM   /* NJTRST */
#define PB4_AF_TIM3_CH1          AF2_TIM_3_5
#define PB4_AF_SPI1_MISO         AF5_SPI_1_4
#define PB4_AF_SPI3_MISO         AF6_SPI_3
#define PB4_AF_EVENTOUT          AF15_EVENTOUT

// PB5
#define PB5_AF_SYSTEM            AF0_SYSTEM
#define PB5_AF_TIM3_CH2          AF2_TIM_3_5
#define PB5_AF_I2C1_SMBA         AF4_I2C_1_3
#define PB5_AF_SPI1_MOSI         AF5_SPI_1_4
#define PB5_AF_SPI3_MOSI         AF6_SPI_3
#define PB5_AF_EVENTOUT          AF15_EVENTOUT

// PB6
#define PB6_AF_SYSTEM            AF0_SYSTEM
#define PB6_AF_TIM4_CH1          AF2_TIM_3_5
#define PB6_AF_I2C1_SCL          AF4_I2C_1_3
#define PB6_AF_USART1_TX         AF7_USART_1_2
#define PB6_AF_EVENTOUT          AF15_EVENTOUT

// PB7
#define PB7_AF_SYSTEM            AF0_SYSTEM
#define PB7_AF_TIM4_CH2          AF2_TIM_3_5
#define PB7_AF_I2C1_SDA          AF4_I2C_1_3
#define PB7_AF_USART1_RX         AF7_USART_1_2
#define PB7_AF_EVENTOUT          AF15_EVENTOUT

// PB8
#define PB8_AF_SYSTEM            AF0_SYSTEM
#define PB8_AF_TIM4_CH3          AF2_TIM_3_5
#define PB8_AF_TIM10_CH1         AF3_TIM_9_11
#define PB8_AF_I2C1_SCL_ALT      AF4_I2C_1_3
#define PB8_AF_EVENTOUT          AF15_EVENTOUT

// PB9
#define PB9_AF_SYSTEM            AF0_SYSTEM
#define PB9_AF_TIM4_CH4          AF2_TIM_3_5
#define PB9_AF_TIM11_CH1         AF3_TIM_9_11
#define PB9_AF_I2C1_SDA_ALT      AF4_I2C_1_3
#define PB9_AF_EVENTOUT          AF15_EVENTOUT

// PB10
#define PB10_AF_SYSTEM           AF0_SYSTEM
#define PB10_AF_I2C2_SCL         AF4_I2C_1_3
#define PB10_AF_EVENTOUT         AF15_EVENTOUT

// PB11
#define PB11_AF_SYSTEM           AF0_SYSTEM
#define PB11_AF_I2C2_SDA         AF4_I2C_1_3
#define PB11_AF_EVENTOUT         AF15_EVENTOUT

// PB12
#define PB12_AF_SYSTEM           AF0_SYSTEM
#define PB12_AF_TIM1_BKIN        AF1_TIM_1_2
#define PB12_AF_I2C2_SMBA        AF4_I2C_1_3
#define PB12_AF_SPI2_NSS         AF5_SPI_1_4
#define PB12_AF_EVENTOUT         AF15_EVENTOUT

// PB13
#define PB13_AF_SYSTEM           AF0_SYSTEM
#define PB13_AF_TIM1_CH1N        AF1_TIM_1_2
#define PB13_AF_SPI2_SCK         AF5_SPI_1_4
#define PB13_AF_EVENTOUT         AF15_EVENTOUT

// PB14
#define PB14_AF_SYSTEM           AF0_SYSTEM
#define PB14_AF_TIM1_CH2N        AF1_TIM_1_2
#define PB14_AF_SPI2_MISO        AF5_SPI_1_4
#define PB14_AF_EVENTOUT         AF15_EVENTOUT

// PB15
#define PB15_AF_SYSTEM           AF0_SYSTEM
#define PB15_AF_TIM1_CH3N        AF1_TIM_1_2
#define PB15_AF_SPI2_MOSI        AF5_SPI_1_4
#define PB15_AF_EVENTOUT         AF15_EVENTOUT

/* ======================= PORT C ======================= */
/* On STM32F401CC (LQFP-48) only PC13–PC15 are bonded out. */

// PC13
#define PC13_AF_SYSTEM           AF0_SYSTEM   /* TAMPER/RTC, user LED on many boards */
#define PC13_AF_EVENTOUT         AF15_EVENTOUT

// PC14
#define PC14_AF_SYSTEM           AF0_SYSTEM   /* OSC32_IN */
#define PC14_AF_EVENTOUT         AF15_EVENTOUT

// PC15
#define PC15_AF_SYSTEM           AF0_SYSTEM   /* OSC32_OUT */
#define PC15_AF_EVENTOUT         AF15_EVENTOUT


#endif /* _GPIO_REGS_H_ */
