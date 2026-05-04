#ifndef _RCC_INTERFACE_H
#define _RCC_INTERFACE_H

#include "STD_TYPES.h"

/* ==================== Error Codes ==================== */
typedef enum
{
    RCC_OK                    = 0x00U,  /* Operation successful */
    RCC_ERROR_INVALID_PARAM   = 0x01U,  /* Invalid parameter passed */
    RCC_ERROR_CLK_ACTIVE      = 0x02U,  /* Trying to disable active system clock */
    RCC_ERROR_CLK_NOT_READY   = 0x03U,  /* Clock not ready within timeout */
    RCC_ERROR_CLK_CONFIG      = 0x04U,  /* Clock configuration error */
    RCC_ERROR_HSE_FAILED      = 0x05U,  /* HSE oscillator failed */
    RCC_ERROR_HSI_FAILED      = 0x06U,  /* HSI oscillator failed */
    RCC_ERROR_PLL_FAILED      = 0x07U,  /* PLL lock failed */
    RCC_ERROR_PERIPH_IN_USE   = 0x08U,  /* Peripheral using clock, cannot disable */
    RCC_ERROR_TIMEOUT         = 0x09U,  /* Operation timeout */
    RCC_ERROR_INVALID_CLK_SRC = 0x0AU,  /* Invalid clock source — explicit per MISRA Rule 8.12 */
    RCC_ERROR_UNKNOWN         = 0xFFU   /* Unknown error */
} RCC_Error_t;

/* ==================== System Clock Source ==================== */
typedef enum
{
    CLK_HSI = 0U,
    CLK_HSE = 1U,
    CLK_PLL = 2U,
    CLK_MAX = 3U
} RCC_SYSCLK_TYPE_t;

/* ==================== PLL Source ==================== */
typedef enum
{
    PLL_SRC_HSI = 0U,
    PLL_SRC_HSE = 1U
} RCC_PLL_SOURCE_t;

/* ==================== Clock Control ==================== */
typedef enum
{
    CLK_ENABLE  = 1U,
    CLK_DISABLE = 0U
} RCC_CLK_CTRL_t;

/* ==================== PLL_M Divider ==================== *
 * All values explicit — no implicit increment.
 * Implicit increment was the root cause of PLLM_16 resolving
 * to 13 instead of 16, causing VCO_INPUT = 1230769 Hz
 * instead of 1000000 Hz and failing the range check.
 * Hardware valid range: 2–63 (RM0368).
 * Values below cover all practical use cases.
 * ======================================================== */
typedef enum
{
    PLLM_2  =  2U,
    PLLM_3  =  3U,
    PLLM_4  =  4U,
    PLLM_5  =  5U,
    PLLM_6  =  6U,
    PLLM_7  =  7U,
    PLLM_8  =  8U,
    PLLM_9  =  9U,
    PLLM_10 = 10U,
    PLLM_11 = 11U,
    PLLM_12 = 12U,
    PLLM_13 = 13U,
    PLLM_14 = 14U,
    PLLM_15 = 15U,
    PLLM_16 = 16U,
    PLLM_17 = 17U,
    PLLM_18 = 18U,
    PLLM_19 = 19U,
    PLLM_20 = 20U,
    PLLM_21 = 21U,
    PLLM_22 = 22U,
    PLLM_23 = 23U,
    PLLM_24 = 24U,
    PLLM_25 = 25U
} PLLM_CFG_t;

/* ==================== PLL_N Multiplier ==================== *
 * All values explicit per MISRA Rule 8.12.
 * Hardware valid range: 192–432 (RM0368).
 * Only commonly used values listed — add more if needed.
 * ========================================================== */
typedef enum
{
    PLLN_192 = 192U,
    PLLN_216 = 216U,
    PLLN_240 = 240U,
    PLLN_288 = 288U,
    PLLN_336 = 336U,
    PLLN_384 = 384U,
    PLLN_400 = 400U,
    PLLN_432 = 432U
} PLLN_CFG_t;

/* ==================== PLL_P Divider ==================== *
 * Only 4 valid values per hardware (RM0368 §6.3.2).
 * All explicit.
 * ======================================================= */
typedef enum
{
    PLLP_2 = 2U,
    PLLP_4 = 4U,
    PLLP_6 = 6U,
    PLLP_8 = 8U
} PLLP_CFG_t;

/* ==================== PLL_Q Divider ==================== *
 * Used for USB/SDIO/RNG clock = VCO / PLLQ.
 * USB enforcement removed (project does not use USB) but
 * PLLQ must still be written to a valid register value.
 * Hardware valid range: 2–15 (RM0368 §6.3.2).
 * All values explicit per MISRA Rule 8.12.
 * ======================================================= */
typedef enum
{
    PLLQ_2  =  2U,
    PLLQ_3  =  3U,
    PLLQ_4  =  4U,
    PLLQ_5  =  5U,
    PLLQ_6  =  6U,
    PLLQ_7  =  7U,
    PLLQ_8  =  8U,
    PLLQ_9  =  9U,
    PLLQ_10 = 10U,
    PLLQ_11 = 11U,
    PLLQ_12 = 12U,
    PLLQ_13 = 13U,
    PLLQ_14 = 14U,
    PLLQ_15 = 15U
} PLLQ_CFG_t;

/* ==================== PLL Config Struct ==================== */
typedef struct
{
    RCC_PLL_SOURCE_t PLL_Source;
    PLLM_CFG_t       PLL_M;
    PLLN_CFG_t       PLL_N;
    PLLP_CFG_t       PLL_P;
    PLLQ_CFG_t       PLL_Q;
} PLL_CFG_t;

/* ==================== Peripheral Clock IDs ==================== *
 * Encoding: bits[6:5] = bus index, bits[4:0] = ENR bit position
 *
 * Bus index:
 *   0x00 range → AHB1 (bus 0)
 *   0x20 range → AHB2 (bus 1)
 *   0x40 range → APB1 (bus 2)
 *   0x60 range → APB2 (bus 3)
 * ============================================================== */
typedef enum
{
    /* ---- AHB1 (bus = 0) ---- */
    PERIPH_GPIOA  = 0x00U,  /* AHB1ENR bit 0  */
    PERIPH_GPIOB  = 0x01U,  /* AHB1ENR bit 1  */
    PERIPH_GPIOC  = 0x02U,  /* AHB1ENR bit 2  */
    PERIPH_GPIOD  = 0x03U,  /* AHB1ENR bit 3  */
    PERIPH_GPIOE  = 0x04U,  /* AHB1ENR bit 4  */
    PERIPH_GPIOH  = 0x07U,  /* AHB1ENR bit 7  */
    PERIPH_CRC    = 0x0CU,  /* AHB1ENR bit 12 */
    PERIPH_DMA1   = 0x15U,  /* AHB1ENR bit 21 */
    PERIPH_DMA2   = 0x16U,  /* AHB1ENR bit 22 */

    /* ---- AHB2 (bus = 1) ---- */
    PERIPH_OTGFS  = 0x27U,  /* AHB2ENR bit 7  */

    /* ---- APB1 (bus = 2) ---- */
    PERIPH_TIM2   = 0x40U,  /* APB1ENR bit 0  */
    PERIPH_TIM3   = 0x41U,  /* APB1ENR bit 1  */
    PERIPH_TIM4   = 0x42U,  /* APB1ENR bit 2  */
    PERIPH_TIM5   = 0x43U,  /* APB1ENR bit 3  */
    PERIPH_WWDG   = 0x4BU,  /* APB1ENR bit 11 */
    PERIPH_SPI2   = 0x4EU,  /* APB1ENR bit 14 */
    PERIPH_SPI3   = 0x4FU,  /* APB1ENR bit 15 */
    PERIPH_USART2 = 0x51U,  /* APB1ENR bit 17 */
    PERIPH_I2C1   = 0x55U,  /* APB1ENR bit 21 */
    PERIPH_I2C2   = 0x56U,  /* APB1ENR bit 22 */
    PERIPH_I2C3   = 0x57U,  /* APB1ENR bit 23 */
    PERIPH_PWR    = 0x5CU,  /* APB1ENR bit 28 */

    /* ---- APB2 (bus = 3) ---- */
    PERIPH_TIM1   = 0x60U,  /* APB2ENR bit 0  */
    PERIPH_USART1 = 0x64U,  /* APB2ENR bit 4  */
    PERIPH_USART6 = 0x65U,  /* APB2ENR bit 5  */
    PERIPH_ADC1   = 0x68U,  /* APB2ENR bit 8  */
    PERIPH_SDIO   = 0x6BU,  /* APB2ENR bit 11 */
    PERIPH_SPI1   = 0x6CU,  /* APB2ENR bit 12 */
    PERIPH_SPI4   = 0x6DU,  /* APB2ENR bit 13 */
    PERIPH_SYSCFG = 0x6EU,  /* APB2ENR bit 14 */
    PERIPH_TIM9   = 0x70U,  /* APB2ENR bit 16 */
    PERIPH_TIM10  = 0x71U,  /* APB2ENR bit 17 */
    PERIPH_TIM11  = 0x72U   /* APB2ENR bit 18 */

} RCC_Peripheral_t;

/* ==================== Prescaler Enums (reserved) ==================== *
 * Reserved for future RCC_SetAPBPrescaler function — not yet implemented.
 * Values are the 3-bit PPRE field encodings from RM0368 §6.3.3, not
 * the actual divisor values. Do not use as divisors directly.
 * ==================================================================== */
typedef enum
{
    APB_PRESCALER_1  = 0x0U,
    APB_PRESCALER_2  = 0x4U,
    APB_PRESCALER_4  = 0x5U,
    APB_PRESCALER_8  = 0x6U,
    APB_PRESCALER_16 = 0x7U
} RCC_APB_PRESCALER_t;

/* Reserved for future RCC_SetAHBPrescaler function — not yet implemented.
 * Values are the 4-bit HPRE field encodings from RM0368 §6.3.3.
 * Do not use as divisors directly. */
typedef enum
{
    AHB_PRESCALER_1   = 0x0U,
    AHB_PRESCALER_2   = 0x8U,
    AHB_PRESCALER_4   = 0x9U,
    AHB_PRESCALER_8   = 0xAU,
    AHB_PRESCALER_16  = 0xBU,
    AHB_PRESCALER_64  = 0xCU,
    AHB_PRESCALER_128 = 0xDU,
    AHB_PRESCALER_256 = 0xEU,
    AHB_PRESCALER_512 = 0xFU
} RCC_AHB_PRESCALER_t;

/* ==================== Function Prototypes ==================== */

RCC_Error_t RCC_SET_SYSCLK(RCC_SYSCLK_TYPE_t clk_type);
RCC_Error_t RCC_CTL_CLK(RCC_SYSCLK_TYPE_t clk_type, RCC_CLK_CTRL_t state);
RCC_Error_t RCC_PLL_CFG(PLL_CFG_t *PLL_CFG);
RCC_Error_t RCC_PLL_Enable(void);
RCC_Error_t RCC_EN_CLK_PERIPHERAL(RCC_Peripheral_t peri_clk);
RCC_Error_t RCC_DisablePeripheralClock(RCC_Peripheral_t peri_clk);
RCC_Error_t get_sys_clk(RCC_SYSCLK_TYPE_t *clk_src);
RCC_Error_t GET_AHB_FREQ(uint32_t *freq);
RCC_Error_t GET_PCLK_FREQ(RCC_Peripheral_t peri, uint32_t *freq);
RCC_Error_t RCC_GET_PLL_FREQ(uint32_t *freq);
RCC_Error_t RCC_GET_SYSCLK_FREQ(uint32_t *freq);
RCC_Error_t RCC_INIT_84MHz_HSI(void);

#endif /* _RCC_INTERFACE_H */