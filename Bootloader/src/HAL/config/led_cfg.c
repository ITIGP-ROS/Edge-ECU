#include "configuration/HAL/led_cfg.h"
#include "interface/HAL/led.h"

#include "interface/MCAL/gpio.h"

const LED_Config_t LEDS[LED_LEN] = {
    [LED_0] = { .port = GPIO_PORTA, .pin = GPIO_PIN_0, .activeState = LED_ACTIVE_HIGH, .isPP = 1 },
    [LED_1] = { .port = GPIO_PORTA, .pin = GPIO_PIN_1, .activeState = LED_ACTIVE_HIGH, .isPP = 1 },
};