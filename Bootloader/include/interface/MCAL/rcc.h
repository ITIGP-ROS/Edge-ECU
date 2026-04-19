#ifndef RCC_H
#define RCC_H

#include "../../../lib/STD_Types.h"

#define RCC_PLL_MAX       0xFF

typedef enum {
    RCC_DISABLE = 0,
    RCC_ENABLE
} RCC_Status_t;

typedef enum {
    RCC_CLOCK_SOURCE_HSI = 0,
    RCC_CLOCK_SOURCE_HSE,
    RCC_CLOCK_SOURCE_PLL
} RCC_ClockType_t;

typedef enum {
    RCC_AHB1 = 0,
    RCC_AHB2,
    RCC_APB1,
    RCC_APB2
} RCC_BusType_t;

typedef enum {
    RCC_PERIPHERAL_DISABLE = 0,
    RCC_PERIPHERAL_ENABLE,
    RCC_PERIPHERAL_RESET
} RCC_Peripheral_Operation_t;

typedef union {
    struct {
        uint16_t PLLN;  // Multiplication factor for the main PLL VCO
        uint8_t PLLM;   // Division factor for the main PLL input clock
        uint8_t PLLP;   // Division factor for main system clock
        uint8_t PLLQ;   // Division factor for USB OTG FS, SDIO and RNG clocks
    } pll_cfg_custom_t;
    struct {
        uint8_t pllMax;
    } pll_cfg_max_t;
} PLL_CFG_t;

typedef struct {
    RCC_ClockType_t sysClkSource;
    RCC_ClockType_t pllClkSource;
    PLL_CFG_t pllConfig;
} RCC_CFG_t;

#define RCC_BUS_MASK        0xC0000000U 
#define RCC_BUS_OFFSET      30
#define RCC_PERIPH_MASK     0x3FFFFFFFU

#define RCC_BUS_AHB1_ID (RCC_AHB1 << RCC_BUS_OFFSET)
#define RCC_BUS_AHB2_ID (RCC_AHB2 << RCC_BUS_OFFSET)
#define RCC_BUS_APB1_ID (RCC_APB1 << RCC_BUS_OFFSET)
#define RCC_BUS_APB2_ID (RCC_APB2 << RCC_BUS_OFFSET)

typedef enum {
    // AHB1 Peripherals (Bus ID 0)
    RCC_GPIOA   = RCC_BUS_AHB1_ID | (1U << 0),
    RCC_GPIOB   = RCC_BUS_AHB1_ID | (1U << 1),
    RCC_GPIOC   = RCC_BUS_AHB1_ID | (1U << 2),
    RCC_GPIOD   = RCC_BUS_AHB1_ID | (1U << 3),
    RCC_GPIOE   = RCC_BUS_AHB1_ID | (1U << 4),
    RCC_GPIOH   = RCC_BUS_AHB1_ID | (1U << 7),
    RCC_CRC     = RCC_BUS_AHB1_ID | (1U << 12),
    RCC_DMA1    = RCC_BUS_AHB1_ID | (1U << 21),
    RCC_DMA2    = RCC_BUS_AHB1_ID | (1U << 22),

    // AHB2 Peripherals (Bus ID 1)
    RCC_OTGFS   = RCC_BUS_AHB2_ID | (1U << 7),

    // APB1 Peripherals (Bus ID 1)
    RCC_TIM2    = RCC_BUS_APB1_ID | (1U << 0),
    RCC_TIM3    = RCC_BUS_APB1_ID | (1U << 1),
    RCC_TIM4    = RCC_BUS_APB1_ID | (1U << 2),
    RCC_TIM5    = RCC_BUS_APB1_ID | (1U << 3),
    RCC_WWDG    = RCC_BUS_APB1_ID | (1U << 11),
    RCC_SPI2    = RCC_BUS_APB1_ID | (1U << 14),
    RCC_SPI3    = RCC_BUS_APB1_ID | (1U << 15),
    RCC_USART2  = RCC_BUS_APB1_ID | (1U << 17),
    RCC_I2C1    = RCC_BUS_APB1_ID | (1U << 21),
    RCC_I2C2    = RCC_BUS_APB1_ID | (1U << 22),
    RCC_I2C3    = RCC_BUS_APB1_ID | (1U << 23),
    RCC_PWR     = RCC_BUS_APB1_ID | (1U << 28),

    // APB2 Peripherals (Bus ID 2)
    RCC_TIM1    = RCC_BUS_APB2_ID | (1U << 0),
    RCC_USART1  = RCC_BUS_APB2_ID | (1U << 4),
    RCC_USART6  = RCC_BUS_APB2_ID | (1U << 5),
    RCC_ADC1    = RCC_BUS_APB2_ID | (1U << 8),
    RCC_SDIO    = RCC_BUS_APB2_ID | (1U << 11),
    RCC_SPI1    = RCC_BUS_APB2_ID | (1U << 12),
    RCC_SPI4    = RCC_BUS_APB2_ID | (1U << 13),
    RCC_SYSCFG  = RCC_BUS_APB2_ID | (1U << 14),
    RCC_TIM9    = RCC_BUS_APB2_ID | (1U << 16),
    RCC_TIM10   = RCC_BUS_APB2_ID | (1U << 17),
    RCC_TIM11   = RCC_BUS_APB2_ID | (1U << 18),
} RCC_Peripheral_t;


typedef enum {
    RCC_AHB_PRESCALER_DIV1 = 0,
    RCC_AHB_PRESCALER_DIV2 = 8,
    RCC_AHB_PRESCALER_DIV4 = 16,
    RCC_AHB_PRESCALER_DIV8 = 24,
    RCC_AHB_PRESCALER_DIV16 = 32,
    RCC_AHB_PRESCALER_DIV64 = 64,
    RCC_AHB_PRESCALER_DIV128 = 128,
    RCC_AHB_PRESCALER_DIV256 = 256,
    RCC_AHB_PRESCALER_DIV512 = 512
} RCC_AHB_Prescaler_t;

typedef enum {
    RCC_APB_PRESCALER_DIV1 = 0,
    RCC_APB_PRESCALER_DIV2 = 4,
    RCC_APB_PRESCALER_DIV4 = 5,
    RCC_APB_PRESCALER_DIV8 = 6,
    RCC_APB_PRESCALER_DIV16 = 7
} RCC_APB_Prescaler_t;


/**
 * @brief  Configures the system clock according to the provided configuration.
 * @param  cfg: Pointer to RCC configuration structure (contains system clock source, pll clock source, pll configurations)
 * @retval STD_SUCCESS if configuration was successful, otherwise STD_ERROR.
 */
STD_ReturnType RCC_ConfigureClock(const RCC_CFG_t *cfg);

/**
 * @brief  Sets the system clock source (HSI, HSE, or PLL).
 * @param  clockType: The desired system clock source.
 * @retval STD_SUCCESS if the clock source was set successfully, otherwise STD_ERROR.
 */
STD_ReturnType RCC_SetSystemClock(RCC_ClockType_t clockType);

/**
 * @brief  Enables or disables the specified clock source.
 * @param  clockType: The clock type to control (HSI, HSE, PLL).
 * @param  status: Desired operation (RCC_ENABLE or RCC_DISABLE).
 * @retval STD_SUCCESS if operation was successful, otherwise STD_ERROR.
 */
STD_ReturnType RCC_SetClock(RCC_ClockType_t clockType, RCC_Status_t status);

/**
 * @brief  Waits until the specified clock source becomes ready or a timeout occurs.
 * @param  clockType: The clock type to check (HSI, HSE, PLL).
 * @param  timeout: Timeout duration in milliseconds.
 * @retval STD_SUCCESS if clock becomes ready within timeout, otherwise STD_ERROR.
 */
STD_ReturnType RCC_WaitForClockReady(RCC_ClockType_t clockType, uint32_t timeout);

/**
 * @brief  Waits until the system clock switch to the specified clock source is completed.
 * @param  clockType: The system clock source to wait for.
 * @param  timeout: Timeout duration in milliseconds.
 * @retval STD_SUCCESS if system clock is ready, otherwise STD_ERROR.
 */
STD_ReturnType RCC_WaitForSysClkReady(RCC_ClockType_t clockType, uint32_t timeout);

/**
 * @brief  Configures the PLL according to the provided parameters.
 * @param  pllCfg: Pointer to PLL configuration structure (contains PLLM, PLLN, PLLP, PLLQ, etc.)
 * @retval STD_SUCCESS if configuration was successful, otherwise STD_ERROR.
 */
STD_ReturnType RCC_ConfigurePLL(const PLL_CFG_t *pllCfg);

/**
 * @brief  Selects the input clock source for the PLL.
 * @param  source: The PLL input source (HSI or HSE).
 * @retval STD_SUCCESS if source was set successfully, otherwise STD_ERROR.
 */
STD_ReturnType RCC_SetPLLClockSource(RCC_ClockType_t source);

/**
 * @brief  Reads the currently configured PLL clock source.
 * @param  source: Pointer to variable to store the current PLL source.
 * @retval STD_SUCCESS if read operation was successful, otherwise STD_ERROR.
 */
STD_ReturnType RCC_CheckPLLClockSource(RCC_ClockType_t *source);

/**
 * @brief  Configures the PLL to the maximum supported system clock frequency.
 * @note   Must be called after enabling the PLL and before switching to it as SYSCLK.
 * @retval STD_SUCCESS if configuration was applied successfully, otherwise STD_ERROR.
 */
STD_ReturnType RCC_SetPLLMaxClock(void);

/**
 * @brief  Controls peripheral clock enable/disable/reset for a specific bus.
 * @param  peripheral: Peripheral identifier (may contain bus information and bitmask).
 * @param  operation: Operation to perform (RCC_PERIPHERAL_ENABLE, DISABLE, or RESET).
 * @retval STD_SUCCESS if operation succeeded, otherwise STD_ERROR.
 */
STD_ReturnType RCC_ControlPeripheral(RCC_Peripheral_t peripheral, RCC_Peripheral_Operation_t operation);

/**
 * @brief  Sets the AHB bus prescaler.
 * @param  prescaler: AHB prescaler value (division factor).
 * @retval STD_SUCCESS if prescaler was set successfully, otherwise STD_ERROR.
 */
STD_ReturnType RCC_SetAHBPrescaler(RCC_AHB_Prescaler_t prescaler);

/**
 * @brief  Sets the APB bus prescaler for APB1 or APB2.
 * @param  prescaler: APB prescaler value (division factor).
 * @param  bus: Bus selection (RCC_APB1 or RCC_APB2).
 * @retval STD_SUCCESS if prescaler was set successfully, otherwise STD_ERROR.
 */
STD_ReturnType RCC_SetAPBPrescaler(RCC_APB_Prescaler_t prescaler, RCC_BusType_t bus);

/**
 * @brief  Enable/Disable HSE bypass mode (used when external clock signal is provided instead of a crystal).
 * @param  status: RCC_ENABLE to enable bypass mode, RCC_DISABLE to disable bypass mode.
 * @retval STD_SUCCESS if bypass mode was set successfully, otherwise STD_ERROR.
 */
STD_ReturnType RCC_SetHSEBypass(RCC_Status_t status);

/**
 * @brief  Enables/Disables the Clock Security System (CSS) to detect HSE failure.
 * @param  status: RCC_ENABLE to enable CSS, RCC_DISABLE to disable CSS.
 * @retval STD_SUCCESS if CSS was set successfully, otherwise STD_ERROR.
 */
STD_ReturnType RCC_SetClockSecurity(RCC_Status_t status);

/*
 * @brief  Enable/Disable the Low-Speed Internal (LSI) oscillator.
 * @param  status: RCC_ENABLE to enable LSI, RCC_DISABLE to disable LSI.
 * @retval STD_SUCCESS if LSI was set successfully, otherwise STD_ERROR.
 */
STD_ReturnType RCC_SetLSI(RCC_Status_t status);

#endif // RCC_H