#include "private/MCAL/timer_priv.h"
#include "interface/MCAL/timer.h"
#include "interface/MCAL/rcc.h"
#include "interface/Core/nvic.h"

static STD_ReturnType Timer_ConfigureGPIO(const Timer_t *timerConfig);

TIM_TypeDef* TIM[4] = {TIM2, TIM3, TIM4, TIM5};
Timer_Callback_t TimerOVF_Callbacks[4] = {NULL, NULL, NULL, NULL}; // Array to hold callbacks for each timer instance
Timer_Callback_t Timer_CC_ChannelsCallbacks[4][4] = {{NULL}};      // 2D Array to hold callbacks for each timer instance and channel

STD_ReturnType Timer_Init(const Timer_t *timerConfig){
    if(timerConfig == NULL){
        return STD_ERROR;
    }
    // Enable the clock for the specified timer
    switch(timerConfig->instance){
        case TIMER_2:
            RCC_ControlPeripheral(RCC_TIM2, RCC_PERIPHERAL_ENABLE);
            break;
        case TIMER_3:
            RCC_ControlPeripheral(RCC_TIM3, RCC_PERIPHERAL_ENABLE);
            break;
        case TIMER_4:
            RCC_ControlPeripheral(RCC_TIM4, RCC_PERIPHERAL_ENABLE);
            break;
        case TIMER_5:
            RCC_ControlPeripheral(RCC_TIM5, RCC_PERIPHERAL_ENABLE);
            break;
        default:
            return STD_ERROR;
    }

    // Configure the timer based on the provided configuration
    Timer_SetMode(timerConfig);
    Timer_SetPrescaler(timerConfig);
    Timer_SetAutoReload(timerConfig);
    // Generate an update event to load the prescaler value immediately
    TIM[timerConfig->instance]->EGR |= (1U << 0);   // (UG Bit) Update Generation
    TIM[timerConfig->instance]->SR  &= ~(1U << 0);  // clear UIF bit - Update Interrupt Flag

    // Set Callbacks
    if(timerConfig->ovfCallback!= NULL){
        TimerOVF_Callbacks[timerConfig->instance] = timerConfig->ovfCallback;
    }

    return STD_SUCCESS;
}

STD_ReturnType Timer_Start(const Timer_t *timerConfig){
    if(timerConfig == NULL){
        return STD_ERROR;
    }
    else{
        TIM[timerConfig->instance]->CR1 |= (1U << 0); // Set the CEN bit to start the timer
    }

    return STD_SUCCESS;
}

STD_ReturnType Timer_Start_IT(const Timer_t *timerConfig){
    if(timerConfig == NULL || timerConfig->ovfCallback == NULL){
        return STD_ERROR;
    }
    else{
        // Enable Timer Update Interrupt
        TIM[timerConfig->instance]->DIER |= (1U << 0);
        // Enable NVIC for Timer
        switch(timerConfig->instance){
            case TIMER_2: NVIC_EnableIRQ(TIM2_IRQn); break;
            case TIMER_3: NVIC_EnableIRQ(TIM3_IRQn); break;
            case TIMER_4: NVIC_EnableIRQ(TIM4_IRQn); break;
            case TIMER_5: NVIC_EnableIRQ(TIM5_IRQn); break;
            default: return STD_ERROR;
        }
        // Start the timer
        TIM[timerConfig->instance]->CR1 |= (1U << 0);
    }

    return STD_SUCCESS;
}

STD_ReturnType Timer_Start_PWM(const Timer_t *timerConfig){
    STD_ReturnType ret = STD_SUCCESS;
    if(timerConfig == NULL){
        ret = STD_ERROR;
    }
    else{
        // Configure GPIO for PWM output
        ret = Timer_ConfigureGPIO(timerConfig);
        if(ret != STD_SUCCESS){
            return ret;
        }
        // Configure channel as PWM output & Set preload & Set PWM mode
        if(timerConfig->channel < TIMER_CHANNEL_3){
            TIM[timerConfig->instance]->CCMR1 &= ~(1U << (timerConfig->channel * 8)); // Clear CCxS bit
            TIM[timerConfig->instance]->CCMR1 |= (1U << (timerConfig->channel * 8 + 3)); // Set CCxPE bit
            TIM[timerConfig->instance]->CCMR1 &= ~(0x7U << (timerConfig->channel * 8 + 4)); // Clear OCxM bits
            TIM[timerConfig->instance]->CCMR1 |= (0x6U << (timerConfig->channel * 8 + 4)); // Set to PWM mode 1
        }
        else{
            TIM[timerConfig->instance]->CCMR2 &= ~(1U << ((timerConfig->channel - 2) * 8)); // Clear CCxS bit
            TIM[timerConfig->instance]->CCMR2 |= (1U << ((timerConfig->channel - 2) * 8 + 3)); // Set CCxPE bit
            TIM[timerConfig->instance]->CCMR2 &= ~(0x7U << ((timerConfig->channel - 2) * 8 + 4)); // Clear OCxM bits
            TIM[timerConfig->instance]->CCMR2 |= (0x6U << ((timerConfig->channel - 2) * 8 + 4)); // Set to PWM mode 1
        }
        // Enable the PWM Channel
        TIM[timerConfig->instance]->CCER |= (1U << (timerConfig->channel * 4));
        // Start the timer
        TIM[timerConfig->instance]->CR1 |= (1U << 0);
    }
    return ret;

}

STD_ReturnType Timer_PWM_SetDutyCycle(const Timer_t *timerConfig, uint8_t dutyCycle){
    if(timerConfig == NULL || dutyCycle > 100){
        return STD_ERROR;
    }
    else{
        uint32_t ccrValue = (TIM[timerConfig->instance]->ARR + 1) * dutyCycle / 100;
        switch(timerConfig->channel){
            case TIMER_CHANNEL_1: TIM[timerConfig->instance]->CCR1 = ccrValue; break;
            case TIMER_CHANNEL_2: TIM[timerConfig->instance]->CCR2 = ccrValue; break;
            case TIMER_CHANNEL_3: TIM[timerConfig->instance]->CCR3 = ccrValue; break;
            case TIMER_CHANNEL_4: TIM[timerConfig->instance]->CCR4 = ccrValue; break;
            default: return STD_ERROR;
        }
    }
    return STD_SUCCESS;
}

STD_ReturnType Timer_Start_IC(const Timer_t *timerConfig){
    STD_ReturnType ret = STD_SUCCESS;
    if(timerConfig == NULL){
        ret = STD_ERROR;
    }
    else{
        // Configure GPIO for Input Capture
        ret = Timer_ConfigureGPIO(timerConfig);
        if(ret != STD_SUCCESS){
            return ret;
        }
        // Configure channel as Input Capture & Set filter & Set IC polarity
        if(timerConfig->channel < TIMER_CHANNEL_3){
            TIM[timerConfig->instance]->CCMR1 &= ~(0xFFU << (timerConfig->channel * 8)); // Clear CCMR1Chx bits
            TIM[timerConfig->instance]->CCMR1 |= (0x1U << (timerConfig->channel * 8)); // Set CCxS to 01 for input capture
            TIM[timerConfig->instance]->CCMR1 |= (((timerConfig->icFilter & 0xFU) << (timerConfig->channel * 8 + 4))); // Set filter
        }
        else{
            TIM[timerConfig->instance]->CCMR2 &= ~(0xFFU << ((timerConfig->channel - 2) * 8)); // Clear CCxS bits
            TIM[timerConfig->instance]->CCMR2 |= (0x1U << ((timerConfig->channel - 2) * 8)); // Set CCxS to 01 for input capture
            TIM[timerConfig->instance]->CCMR2 |= (((timerConfig->icFilter & 0xFU) << ((timerConfig->channel - 2) * 8 + 4))); // Set filter
        }
        TIM[timerConfig->instance]->CCER &= ~(0x5U << (timerConfig->channel * 4 + 1)); // Clear polarity
        TIM[timerConfig->instance]->CCER |= (timerConfig->icPolarity << (timerConfig->channel * 4 + 1)); // Set polarity
        // Enable the Input Capture Channel
        TIM[timerConfig->instance]->CCER |= (1U << (timerConfig->channel * 4));
        // Enable Timer Update Interrupt
        TIM[timerConfig->instance]->DIER |= (1U << (timerConfig->channel + 1)); // Enable CCxIE for the corresponding channel
        // Enable NVIC for Timer
        switch(timerConfig->instance){
            case TIMER_2: NVIC_EnableIRQ(TIM2_IRQn); break;
            case TIMER_3: NVIC_EnableIRQ(TIM3_IRQn); break;
            case TIMER_4: NVIC_EnableIRQ(TIM4_IRQn); break;
            case TIMER_5: NVIC_EnableIRQ(TIM5_IRQn); break;
            default: return STD_ERROR;
        }
        // Set Capture/Compare Callback
        if(timerConfig->ccCallback != NULL){
            Timer_CC_ChannelsCallbacks[timerConfig->instance][timerConfig->channel] = timerConfig->ccCallback;
        }
        // Start the timer
        TIM[timerConfig->instance]->CR1 |= (1U << 0);
    }

    return ret;
}

STD_ReturnType Timer_Stop(const Timer_t *timerConfig){
    if(timerConfig == NULL){
        return STD_ERROR;
    }
    else{
        // Stop the timer
        TIM[timerConfig->instance]->CR1 &= ~(1U << 0);
    }

    return STD_SUCCESS;
}

STD_ReturnType Timer_Stop_IT(const Timer_t *timerConfig){
    if(timerConfig == NULL){
        return STD_ERROR;
    }
    else{
        // Disable Timer Update Interrupt
        TIM[timerConfig->instance]->DIER &= ~(1U << 0);
        // Disable NVIC for Timer
        switch(timerConfig->instance){
            case TIMER_2: NVIC_DisableIRQ(TIM2_IRQn); break;
            case TIMER_3: NVIC_DisableIRQ(TIM3_IRQn); break;
            case TIMER_4: NVIC_DisableIRQ(TIM4_IRQn); break;
            case TIMER_5: NVIC_DisableIRQ(TIM5_IRQn); break;
            default: return STD_ERROR;
        }
        // Stop the timer
        TIM[timerConfig->instance]->CR1 &= ~(1U << 0);
    }

    return STD_SUCCESS;
}

STD_ReturnType Timer_Stop_PWM(const Timer_t *timerConfig){
    if(timerConfig == NULL){
        return STD_ERROR;
    }
    else{
        // Disable the PWM Channel
        TIM[timerConfig->instance]->CCER &= ~(1U << (timerConfig->channel * 4));
    }

    return STD_SUCCESS;
}

STD_ReturnType Timer_Stop_IC(const Timer_t *timerConfig){
    if(timerConfig == NULL){
        return STD_ERROR;
    }
    else{
        // Disable the Input Capture Channel
        TIM[timerConfig->instance]->CCER &= ~(1U << (timerConfig->channel * 4));
        // Disable Timer Capture/Compare Interrupt
        TIM[timerConfig->instance]->DIER &= ~(1U << (timerConfig->channel + 1));
    }

    return STD_SUCCESS;
}

STD_ReturnType Timer_GetCounter(const Timer_t *timerConfig, uint32_t* counterValue){
    if(timerConfig == NULL || counterValue == NULL){
        return STD_ERROR;
    }
    else{
        *counterValue = TIM[timerConfig->instance]->CNT;
    }

    return STD_SUCCESS;
}

STD_ReturnType Timer_SetAutoReload(const Timer_t *timerConfig){
    if(timerConfig == NULL){
        return STD_ERROR;
    }
    else{
        if((timerConfig->instance == TIMER_3 || timerConfig->instance == TIMER_4) && (timerConfig->autoReloadValue > 0xFFFF)){
            TIM[timerConfig->instance]->ARR = 0xFFFF; // Set to max for 16-bit timers
        }
        else{
            TIM[timerConfig->instance]->ARR = timerConfig->autoReloadValue;
        }
    }

    return STD_SUCCESS;
}

STD_ReturnType Timer_SetPrescaler(const Timer_t *timerConfig){
    if(timerConfig == NULL){
        return STD_ERROR;
    }
    else{
        TIM[timerConfig->instance]->PSC = timerConfig->prescaler;
    }

    return STD_SUCCESS;
}

STD_ReturnType Timer_SetMode(const Timer_t *timerConfig){
    if(timerConfig == NULL){
        return STD_ERROR;
    }
    else{
        if(timerConfig->mode == TIMER_MODE_UP || timerConfig->mode == TIMER_MODE_DOWN){
            TIM[timerConfig->instance]->CR1 &= ~(0x3U << 5); // Clear the CMS bits
            TIM[timerConfig->instance]->CR1 &= ~(1U << 4);
            TIM[timerConfig->instance]->CR1 |= (timerConfig->mode << 4); // 0 => up, 1 => down
        }
        else{
            TIM[timerConfig->instance]->CR1 &= ~(0x3U << 5);
            TIM[timerConfig->instance]->CR1 |= ((timerConfig->mode - 1) << 5);  // Set the CMS bits based on the center-aligned mode
        }
    }

    return STD_SUCCESS;
}

void TIM_IRQ_Handler(Timer_Instance_t timerInstance){
    if(timerInstance > TIMER_5){
        return;
    }
    TIM_TypeDef* timer = TIM[timerInstance];
    // Check for update interrupt
    if(timer->SR & (1U << 0)){ 
        timer->SR &= ~(1U << 0); // Clear the update interrupt flag
        if(TimerOVF_Callbacks[timerInstance] != NULL){
            TimerOVF_Callbacks[timerInstance](); // Call callback function
        }
    }
    
    // Check for Capture/Compare interrupts for each channel
    for(uint8_t channel = 0; channel < 4; channel++){
        // Check CC interrupt enable & flag
        if(timer->DIER & (1U << (channel + 1)) && timer->SR & (1U << (channel + 1))){
            timer->SR &= ~(1U << (channel + 1)); // Clear CCxIF flag
            // You can Read the captured value from CCRx register
            if(Timer_CC_ChannelsCallbacks[timerInstance][channel] != NULL){
                Timer_CC_ChannelsCallbacks[timerInstance][channel](); // Call channel callback function
            }
        }
    }
}

static STD_ReturnType Timer_ConfigureGPIO(const Timer_t *timerConfig){
    GPIO_t pinConfig;
    pinConfig.port = timerConfig->port;
    pinConfig.pin = timerConfig->pin; 
    pinConfig.mode = GPIO_MODE_AF;
    pinConfig.outputType = GPIO_OUTPUT_PUSHPULL;
    pinConfig.pullType = timerConfig->pullType;
    pinConfig.speed = GPIO_SPEED_HIGH;
    pinConfig.altFunc = (timerConfig->instance == TIMER_2) ? 
                            GPIO_AF1_TIM1_2 : GPIO_AF2_TIM3_4_5;

    return GPIO_Init(&pinConfig);
}

void TIM2_IRQHandler(void){ TIM_IRQ_Handler(TIMER_2); }
void TIM3_IRQHandler(void){ TIM_IRQ_Handler(TIMER_3); }
void TIM4_IRQHandler(void){ TIM_IRQ_Handler(TIMER_4); }
void TIM5_IRQHandler(void){ TIM_IRQ_Handler(TIMER_5); }
