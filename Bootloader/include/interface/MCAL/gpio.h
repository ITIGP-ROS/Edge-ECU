#include "../../../lib/STD_Types.h"

#ifndef _GPIO_H_
#define _GPIO_H_

#define GPIO_MODER_MASK       0x3U
#define GPIO_SPEED_MASK       0x3U
#define GPIO_PULL_MASK        0x3U
#define GPIO_ALTF_MASK        0xFU

/*
write/toggle port
*/

typedef enum {
    GPIO_PORTA = 0,
    GPIO_PORTB,
    GPIO_PORTC,
    GPIO_PORTD,
    GPIO_PORTE
} GPIO_Port_t;

typedef enum {
    GPIO_PIN_0 = 0,
    GPIO_PIN_1,
    GPIO_PIN_2,
    GPIO_PIN_3,
    GPIO_PIN_4,
    GPIO_PIN_5,
    GPIO_PIN_6,
    GPIO_PIN_7,
    GPIO_PIN_8,
    GPIO_PIN_9,
    GPIO_PIN_10,
    GPIO_PIN_11,
    GPIO_PIN_12,
    GPIO_PIN_13,
    GPIO_PIN_14,
    GPIO_PIN_15,
    GPIO_PIN_ALL = 0xFFFF
} GPIO_Pin_t;

typedef enum {
    GPIO_MODE_INPUT = 0x0U,
    GPIO_MODE_OUTPUT,
    GPIO_MODE_AF,
    GPIO_MODE_ANALOG
} GPIO_Mode_t;


typedef enum {
    GPIO_OUTPUT_PUSHPULL = 0x0U,
    GPIO_OUTPUT_OPENDRAIN
} GPIO_OutputType_t;

typedef enum {
    GPIO_SPEED_LOW = 0x0U,
    GPIO_SPEED_MEDIUM,
    GPIO_SPEED_HIGH,
    GPIO_SPEED_VERY_HIGH
} GPIO_Speed_t;

typedef enum {
    GPIO_NOPULL = 0x0U,
    GPIO_PULLUP,
    GPIO_PULLDOWN
} GPIO_Pull_t;

typedef enum {
    GPIO_PIN_RESET = 0x0U,
    GPIO_PIN_SET
} GPIO_PinState_t;

typedef enum {
    GPIO_AF0_SYSTEM       = 0x0U, // Default AF -> GPIO
    GPIO_AF1_TIM1_2,
    GPIO_AF2_TIM3_4_5,
    GPIO_AF3_TIM9_10_11,
    GPIO_AF4_I2C1_2_3,
    GPIO_AF5_SPI1_2,
    GPIO_AF6_SPI3,
    GPIO_AF7_USART1_2,
    GPIO_AF8_USART6,
    GPIO_AF9_I2C2_3,
    GPIO_AF10_OTG_FS,
    GPIO_AF12_SDIO        = 0xCU,  
    GPIO_AF15_EVENTOUT    = 0xFU
} GPIO_AltFunc_t;

typedef struct{
    GPIO_Port_t port;
    GPIO_Pin_t pin;
    GPIO_Mode_t mode;
    GPIO_Speed_t speed;
    GPIO_OutputType_t outputType;
    GPIO_Pull_t pullType;
    GPIO_AltFunc_t altFunc;
} GPIO_t;

/**
 * @brief Initializes the specified GPIO pin with the given configuration.
 * @param gpio: Pointer to the GPIO_t structure containing the pin configuration.
 * @retval STD_SUCCESS if initialization was successful, otherwise STD_ERROR.
 */
STD_ReturnType GPIO_Init(GPIO_t* gpio);

/**
 * @brief De-initializes the specified GPIO pin, resetting it to its default state.
 * @param gpio: Pointer to the GPIO_t structure containing the pin configuration.
 * @retval STD_SUCCESS if de-initialization was successful, otherwise STD_ERROR.
 */
STD_ReturnType GPIO_DeInit(GPIO_t* gpio);

/**
 * @brief Sets the mode of the specified GPIO pin.
 * @param gpio: Pointer to the GPIO_t structure containing the pin configuration.
 * @param mode: The desired mode to set.
 * @retval STD_SUCCESS if the operation was successful, otherwise STD_ERROR.
 */
STD_ReturnType GPIO_SetMode(GPIO_t* gpio, GPIO_Mode_t mode);

/**
 * @brief Sets the speed of the specified GPIO pin.
 * @param gpio: Pointer to the GPIO_t structure containing the pin configuration.
 * @param speed: The desired speed to set.
 * @retval STD_SUCCESS if the operation was successful, otherwise STD_ERROR.
 */
STD_ReturnType GPIO_SetSpeed(GPIO_t* gpio, GPIO_Speed_t speed);

/**
 * @brief Sets the pull-up/pull-down configuration of the specified GPIO pin.
 * @param gpio: Pointer to the GPIO_t structure containing the pin configuration.
 * @param pull: The desired pull configuration to set.
 * @retval STD_SUCCESS if the operation was successful, otherwise STD_ERROR.
 */
STD_ReturnType GPIO_SetPull(GPIO_t* gpio, GPIO_Pull_t pull);

/**
 * @brief Sets the output type of the specified GPIO pin.
 * @param gpio: Pointer to the GPIO_t structure containing the pin configuration.
 * @param outputType: The desired output type to set.
 * @retval STD_SUCCESS if the operation was successful, otherwise STD_ERROR.
 */
STD_ReturnType GPIO_SetOutputType(GPIO_t* gpio, GPIO_OutputType_t outputType);

/**
 * @brief Sets the alternate function of the specified GPIO pin.
 * @param gpio: Pointer to the GPIO_t structure containing the pin configuration.
 * @param altFunc: The desired alternate function to set.
 * @retval STD_SUCCESS if the operation was successful, otherwise STD_ERROR.
 */
STD_ReturnType GPIO_SetAltFunction(GPIO_t* gpio, GPIO_AltFunc_t altFunc);

/**
 * @brief Writes a value to the specified GPIO pin.
 * @param gpio: Pointer to the GPIO_t structure containing the pin configuration.
 * @param state: The desired pin state to set (GPIO_PIN_SET or GPIO_PIN_RESET).
 * @retval STD_SUCCESS if the operation was successful, otherwise STD_ERROR.
 */
STD_ReturnType GPIO_WritePin(GPIO_t* gpio, GPIO_PinState_t state);

/**
 * @brief Reads the current state of the specified GPIO pin.
 * @param gpio: Pointer to the GPIO_t structure containing the pin configuration.
 * @param state: Pointer to store the read pin state (GPIO_PIN_SET or GPIO_PIN_RESET).
 * @retval STD_SUCCESS if the operation was successful, otherwise STD_ERROR.
 */
STD_ReturnType GPIO_ReadPin(GPIO_t* gpio, GPIO_PinState_t* state);

/**
 * @brief Toggles the current state of the specified GPIO pin.
 * @param gpio: Pointer to the GPIO_t structure containing the pin configuration.
 * @retval STD_SUCCESS if the operation was successful, otherwise STD_ERROR.
 */
STD_ReturnType GPIO_TogglePin(GPIO_t* gpio);

#endif /* _GPIO_H */