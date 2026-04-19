#include "private/MCAL/gpio_priv.h"
#include "interface/MCAL/gpio.h"

#define GPIO_PORT_MAX 5

// Array of GPIO pointers
GPIOx_t* const GPIO_PORTS[GPIO_PORT_MAX] = {GPIOA, GPIOB, GPIOC, GPIOD, GPIOE};

STD_ReturnType GPIO_Init(GPIO_t* gpio){
    STD_ReturnType ret = STD_SUCCESS;
    if (gpio == NULL || gpio->port >= GPIO_PORT_MAX) {
        return STD_ERROR;
    }
    else{
        // Configure MODER
        ret = GPIO_SetMode(gpio, gpio->mode);
        if(ret == STD_SUCCESS){
            if(gpio->mode == GPIO_MODE_OUTPUT){
                // For output mode, set default ODR to low
                ret = GPIO_WritePin(gpio, GPIO_PIN_RESET);
                // Configure OTYPER
                ret |= GPIO_SetOutputType(gpio, gpio->outputType);
                // Configure OSPEEDR
                ret |= GPIO_SetSpeed(gpio, gpio->speed);
            }
            else if(gpio->mode == GPIO_MODE_AF){
                // For AF mode, configure OTYPER
                ret |= GPIO_SetOutputType(gpio, gpio->outputType);
                // Configure OSPEEDR
                ret |= GPIO_SetSpeed(gpio, gpio->speed);
            }
            else{
                // Nothing
            }
        }
        else{
            // Nothing
        }
        // Configure PUPDR
        if(ret == STD_SUCCESS){
            ret = GPIO_SetPull(gpio, gpio->pullType);
        }
        else{
            // Nothing
        }
        // Configure AFR
        if(ret == STD_SUCCESS && gpio->mode == GPIO_MODE_AF){
            ret = GPIO_SetAltFunction(gpio, gpio->altFunc);
        }
        else{
            // Nothing
        }
    }
    return ret;
}

STD_ReturnType GPIO_DeInit(GPIO_t* gpio) {
    STD_ReturnType ret = STD_SUCCESS;

    if (gpio == NULL || gpio->port >= GPIO_PORT_MAX) {
        return STD_ERROR;
    }

    GPIOx_t* GPIOx = GPIO_PORTS[gpio->port];
    uint32_t pin = gpio->pin;

    if (GPIOx == NULL) {
        return STD_ERROR;
    }
    // Reset MODER (set as input mode = 00)
    GPIOx->MODER.REG &= ~(GPIO_MODER_MASK << (pin * 2));
    // Reset OTYPER (push-pull = 0)
    GPIOx->OTYPER.REG &= ~(1U << pin);
    // Reset OSPEEDR (low speed = 00)
    GPIOx->OSPEEDR.REG &= ~(GPIO_SPEED_MASK << (pin * 2));
    // Reset PUPDR (no pull = 00)
    GPIOx->PUPDR.REG &= ~(GPIO_PULL_MASK << (pin * 2));
    // Reset AFRL/AFRH (AF0 = 0000)
    if (pin < 8) {
        GPIOx->AFRL.REG &= ~(GPIO_ALTF_MASK << (pin * 4));
    } else {
        GPIOx->AFRH.REG &= ~(GPIO_ALTF_MASK << ((pin - 8) * 4));
    }
    // Reset output data (ODR = 0)
    GPIOx->ODR.REG &= ~(1U << pin);

    return ret;
}

STD_ReturnType GPIO_SetMode(GPIO_t* gpio, GPIO_Mode_t mode){
    STD_ReturnType ret = STD_SUCCESS;
    if (gpio == NULL || gpio->port >= GPIO_PORT_MAX) {
        return STD_ERROR;
    }
    else{
        GPIOx_t* GPIOx = GPIO_PORTS[gpio->port];
        gpio->mode = mode;
        GPIOx->MODER.REG &= ~((GPIO_MODER_MASK) << (gpio->pin * 2));
        GPIOx->MODER.REG |= ((mode & GPIO_MODER_MASK) << (gpio->pin * 2));
    }
    return ret;
}

STD_ReturnType GPIO_SetSpeed(GPIO_t* gpio, GPIO_Speed_t speed){
     STD_ReturnType ret = STD_SUCCESS;
    if (gpio == NULL || gpio->port >= GPIO_PORT_MAX) {
        return STD_ERROR;
    }
    else{
        GPIOx_t* GPIOx = GPIO_PORTS[gpio->port];
        gpio->speed = speed;
        GPIOx->OSPEEDR.REG &= ~((GPIO_SPEED_MASK) << (gpio->pin * 2));
        GPIOx->OSPEEDR.REG |= ((speed & GPIO_SPEED_MASK) << (gpio->pin * 2));
    }
    return ret;
}

STD_ReturnType GPIO_SetPull(GPIO_t* gpio, GPIO_Pull_t pull){
        STD_ReturnType ret = STD_SUCCESS;
    if(gpio == NULL || gpio->port >= GPIO_PORT_MAX){
        ret = STD_ERROR;
    }
    else{
        GPIOx_t* GPIOx = GPIO_PORTS[gpio->port];
        gpio->pullType = pull;
        GPIOx->PUPDR.REG &= ~((GPIO_PULL_MASK) << (gpio->pin * 2));
        GPIOx->PUPDR.REG |= ((pull & GPIO_PULL_MASK) << (gpio->pin * 2));
    }
    return ret;
}

STD_ReturnType GPIO_SetOutputType(GPIO_t* gpio, GPIO_OutputType_t outputType){
    STD_ReturnType ret = STD_SUCCESS;
    if(gpio == NULL || gpio->port >= GPIO_PORT_MAX){
        ret = STD_ERROR;
    }
    else{
        gpio->outputType = outputType;
        GPIOx_t* GPIOx = GPIO_PORTS[gpio->port];
        GPIOx->OTYPER.REG &= ~(1U << gpio->pin);
        GPIOx->OTYPER.REG |= ((outputType & 0x1U) << gpio->pin);
    }
    return ret;
}


STD_ReturnType GPIO_SetAltFunction(GPIO_t* gpio, GPIO_AltFunc_t altFunc){
    STD_ReturnType ret = STD_SUCCESS;
    if(gpio == NULL || gpio->port >= GPIO_PORT_MAX){
        ret = STD_ERROR;
    }
    else{
        GPIOx_t* GPIOx = GPIO_PORTS[gpio->port];
        gpio->altFunc = altFunc;
        if(gpio->pin < 8){
            GPIOx->AFRL.REG &= ~((GPIO_ALTF_MASK) << (gpio->pin * 4));
            GPIOx->AFRL.REG |= ((altFunc & GPIO_ALTF_MASK) << (gpio->pin * 4));
        }
        else{
            GPIOx->AFRH.REG &= ~((GPIO_ALTF_MASK) << ((gpio->pin - 8) * 4));
            GPIOx->AFRH.REG |= ((altFunc & GPIO_ALTF_MASK) << ((gpio->pin - 8) * 4));
        }
    }
    return ret;
}

STD_ReturnType GPIO_WritePin(GPIO_t* gpio, GPIO_PinState_t state){
    STD_ReturnType ret = STD_SUCCESS;
    if(gpio == NULL || gpio->port >= GPIO_PORT_MAX){
        ret = STD_ERROR;
    }
    else{
        GPIOx_t* GPIOx = GPIO_PORTS[gpio->port];
        uint32_t pinMask = (1U << gpio->pin);
        if(state == GPIO_PIN_SET){
            GPIOx->BSRR.REG = pinMask;
        }
        else{
            GPIOx->BSRR.REG = (pinMask << 16);
        }
    }
    return ret;
}

STD_ReturnType GPIO_ReadPin(GPIO_t* gpio, GPIO_PinState_t* state){
    STD_ReturnType ret = STD_SUCCESS;
    if(gpio == NULL || state == NULL || gpio->port >= GPIO_PORT_MAX){
        ret = STD_ERROR;
    }
    else{
        GPIOx_t* GPIOx = GPIO_PORTS[gpio->port];
        uint32_t pinMask = (1U << gpio->pin);
        if((GPIOx->IDR.REG & pinMask) != GPIO_PIN_RESET){
            *state = GPIO_PIN_SET;
        }
        else{
            *state = GPIO_PIN_RESET;
        }
    }
    return ret;
}

STD_ReturnType GPIO_TogglePin(GPIO_t* gpio){
    STD_ReturnType ret = STD_SUCCESS;
    if(gpio == NULL || gpio->port >= GPIO_PORT_MAX){
        ret = STD_ERROR;
    }
    else{
        GPIOx_t* GPIOx = GPIO_PORTS[gpio->port];
        uint32_t pinMask = (1U << gpio->pin);
        // Read current pin state from ODR and toggle using BSRR
        if ((GPIOx->ODR.REG & pinMask) != GPIO_PIN_RESET) {
            GPIOx->BSRR.REG = (pinMask << 16);   // Reset
        } else {
            GPIOx->BSRR.REG = pinMask;           // Set
        }
    }
    return ret;
}