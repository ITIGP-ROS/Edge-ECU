
#include "interface/HAL/led.h"
#include "interface/MCAL/gpio.h"

extern const LED_Config_t LEDS[LED_LEN];

GPIO_t gpio_leds[LED_LEN];

STD_ReturnType LED_Init(void){
    STD_ReturnType ret = STD_SUCCESS;
    for(uint8_t i = 0; i < LED_LEN; i++){
        gpio_leds[i] = (GPIO_t){
            .port       = LEDS[i].port,
            .pin        = LEDS[i].pin,
            .mode       = GPIO_MODE_OUTPUT,
            .outputType = (LEDS[i].isPP) ? GPIO_OUTPUT_PUSHPULL : GPIO_OUTPUT_OPENDRAIN,
            .speed      = GPIO_SPEED_MEDIUM,
            .pullType   = GPIO_NOPULL,
            .altFunc    = GPIO_AF0_SYSTEM
        };

        ret = GPIO_Init(&gpio_leds[i]);
        if(ret != STD_SUCCESS){
            break;
        }
    }
    return ret;
}

STD_ReturnType LED_DeInit(uint8_t ledName){
    STD_ReturnType ret = STD_SUCCESS;
    
    if(ledName >= LED_LEN){
        ret = STD_ERROR;
    }
    else{
        ret = GPIO_DeInit(&gpio_leds[ledName]);
    }
    
    return ret;
}

STD_ReturnType LED_SetState(uint8_t ledName, LED_State_t state){
    STD_ReturnType ret = STD_SUCCESS;
    if(ledName >= LED_LEN){
        ret = STD_ERROR;
    }
    else{
        GPIO_PinState_t gpioState = state ^ LEDS[ledName].activeState;
        ret = GPIO_WritePin(&gpio_leds[ledName], gpioState);
    }
    return ret;
}

STD_ReturnType LED_Toggle(uint8_t ledName){
    STD_ReturnType ret = STD_SUCCESS;
    if(ledName >= LED_LEN){
        ret = STD_ERROR;
    }
    ret = GPIO_TogglePin(&gpio_leds[ledName]);
    return ret;
}