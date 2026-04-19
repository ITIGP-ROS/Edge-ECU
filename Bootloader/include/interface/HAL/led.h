#ifndef LED_H
#define LED_H

#include "../../../lib/STD_Types.h"
#include "configuration/HAL/led_cfg.h"

typedef enum {
    LED_ACTIVE_HIGH = 0,
    LED_ACTIVE_LOW
} LED_ACTIVE_STATE_t;

typedef enum {
    LED_LOW = 0,
    LED_HIGH
} LED_State_t;

typedef struct LED_Config {
    uint8_t port        : 4;
    uint8_t pin         : 4;
    uint8_t activeState : 1;
    uint8_t isPP        : 1;    // 1 for push-pull, 0 for open-drain
} LED_Config_t;

/**
 * @brief Initializes the configured LEDs.
 * @retval STD_SUCCESS if initialization was successful, otherwise STD_ERROR.
 */
STD_ReturnType LED_Init(void);

/**
 * @brief DeInitializes the configured LEDs.
 * @retval STD_SUCCESS if de-initialization was successful, otherwise STD_ERROR.
 */
STD_ReturnType LED_DeInit(uint8_t ledName);

/**
 * @brief Sets the state of the specified LED.
 * @param ledName: The name/index of the LED to set.
 * @param state: The desired state (LED_HIGH or LED_LOW).
 * @retval STD_SUCCESS if the operation was successful, otherwise STD_ERROR.
 */
STD_ReturnType LED_SetState(uint8_t ledName, LED_State_t state);

/**
 * @brief Toggles the state of the specified LED.
 * @param ledName: The name/index of the LED to toggle.
 * @retval STD_SUCCESS if the operation was successful, otherwise STD_ERROR.
 */
STD_ReturnType LED_Toggle(uint8_t ledName);

#endif // LED_H