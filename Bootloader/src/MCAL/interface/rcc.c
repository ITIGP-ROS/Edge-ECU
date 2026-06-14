
#include "interface/MCAL/rcc.h"
#include "private/MCAL/rcc_priv.h"
#include "interface/MCAL/flash.h"

STD_ReturnType RCC_ConfigureClock(const RCC_CFG_t *cfg){
    STD_ReturnType ret = STD_SUCCESS;
    if(cfg == NULL){
        ret = STD_ERROR;
    }
    else{
        if(cfg->sysClkSource == RCC_CLOCK_SOURCE_HSI || cfg->sysClkSource == RCC_CLOCK_SOURCE_HSE){
            ret = RCC_SetClock(cfg->sysClkSource, RCC_ENABLE);
            if(ret == STD_SUCCESS){
                ret = RCC_WaitForClockReady(cfg->sysClkSource, 500);
            }
            else{
                // Do nothing
            }
            // Set AHB & APB Prescalers
            if(ret == STD_SUCCESS){
                ret = RCC_SetAHBPrescaler(RCC_AHB_PRESCALER_DIV1);
            }
            else{
                // Do nothing
            }
            if(ret == STD_SUCCESS){
                ret = RCC_SetAPBPrescaler(RCC_APB_PRESCALER_DIV1, RCC_APB1);
            }
            else{
                // Do nothing
            }
            if(ret == STD_SUCCESS){
                ret = RCC_SetAPBPrescaler(RCC_APB_PRESCALER_DIV1, RCC_APB2);
            }
            else{
                // Do nothing
            }
            if(ret == STD_SUCCESS){
                ret = RCC_SetSystemClock(cfg->sysClkSource);
            }
            else{
                // Do nothing
            }
            if(ret == STD_SUCCESS){
                ret = RCC_WaitForSysClkReady(cfg->sysClkSource, 500);
            }
            else{
                // Do nothing
            }
            // Set Flash Latency
            ret = FLASH_SetLatency(FLASH_LATENCY_0WS);  // 0WS for up to 30 MHz
        }
        else if(cfg->sysClkSource == RCC_CLOCK_SOURCE_PLL){
            ret = RCC_SetClock(cfg->pllClkSource, RCC_ENABLE);
            if(ret == STD_SUCCESS){
                ret = RCC_WaitForClockReady(cfg->pllClkSource, 500);
            }
            else{
                // Do nothing
            }
            // set system clock to pll clock source
            if(ret == STD_SUCCESS){
                ret = RCC_SetSystemClock(cfg->pllClkSource);
            }
            else{
                // Do nothing
            }
            if(ret == STD_SUCCESS){
                ret = RCC_WaitForSysClkReady(cfg->pllClkSource, 500);
            }
            else{
                // Do nothing
            }
            // Disable the PLL before configuring
            if(ret == STD_SUCCESS){
                ret = RCC_SetClock(RCC_CLOCK_SOURCE_PLL, RCC_DISABLE);
            }
            else{
                // Do nothing
            }
            // Configure PLL
            if(ret == STD_SUCCESS){
                ret = RCC_SetPLLClockSource(cfg->pllClkSource);
            }
            else{
                // Do nothing
            }
            // C. Set Flash Latency
            ret = FLASH_SetLatency(FLASH_LATENCY_2WS);  // 2WS from 61 up to 84 MHz

            if(ret == STD_SUCCESS){
                ret = RCC_ConfigurePLL(&(cfg->pllConfig));
            }
            else{
                // Do nothing
            }
            // Enable PLL
            if(ret == STD_SUCCESS){
                ret = RCC_SetClock(RCC_CLOCK_SOURCE_PLL, RCC_ENABLE);
            }
            else{
                // Do nothing
            }
            if(ret == STD_SUCCESS){
                ret = RCC_WaitForClockReady(RCC_CLOCK_SOURCE_PLL, 500);
            }
            else{
                // Do nothing
            }
            // Set AHB & APB Prescalers
            if(ret == STD_SUCCESS){
                ret = RCC_SetAHBPrescaler(RCC_AHB_PRESCALER_DIV1);
            }
            else{
                // Do nothing
            }
            if(ret == STD_SUCCESS){
                ret = RCC_SetAPBPrescaler(RCC_APB_PRESCALER_DIV2, RCC_APB1);
            }
            else{
                // Do nothing
            }
            if(ret == STD_SUCCESS){
                ret = RCC_SetAPBPrescaler(RCC_APB_PRESCALER_DIV1, RCC_APB2);
            }
            else{
                // Do nothing
            }
            // Set System Clock to PLL
            if(ret == STD_SUCCESS){
                ret = RCC_SetSystemClock(cfg->sysClkSource);
            }
            else{
                // Do nothing
            }
            if(ret == STD_SUCCESS){
                ret = RCC_WaitForSysClkReady(cfg->sysClkSource, 500);
            }
            else{
                // Do nothing
            }
        }
        else {
            ret = STD_ERROR;
        }
    }
    return ret;
}

STD_ReturnType RCC_SetSystemClock(RCC_ClockType_t clockType){
    STD_ReturnType ret = STD_SUCCESS;
    switch (clockType){
        case RCC_CLOCK_SOURCE_HSI:
            RCC->CFGR.BITS.SW = 0b00;
            break;
        case RCC_CLOCK_SOURCE_HSE:
            RCC->CFGR.BITS.SW = 0b01;
            break;
        case RCC_CLOCK_SOURCE_PLL:
            RCC->CFGR.BITS.SW = 0b10;
            break;
        default:
            ret = STD_ERROR;
            break;
    }
    return ret;
}

STD_ReturnType RCC_WaitForSysClkReady(RCC_ClockType_t clockType, uint32_t timeout){
    uint32_t tickStart = 0;
    STD_ReturnType ret = STD_SUCCESS;
    switch (clockType){
        case RCC_CLOCK_SOURCE_HSI:
            while ((RCC->CFGR.BITS.SWS != 0b00) && (tickStart < timeout)){
                tickStart++;
            }
            if(timeout == tickStart){
                ret = STD_TIMEOUT;
            }
            else{
                // Do nothing
            }
            break;
        case RCC_CLOCK_SOURCE_HSE:
            while ((RCC->CFGR.BITS.SWS != 0b01) && (tickStart < timeout)){
                tickStart++;
            }
            if(timeout == tickStart){
                ret = STD_TIMEOUT;
            }
            else{
                // Do nothing
            }
            break;
        case RCC_CLOCK_SOURCE_PLL:
            while ((RCC->CFGR.BITS.SWS != 0b10) && (tickStart < timeout)){
                tickStart++;
            }
            if(timeout == tickStart){
                ret = STD_TIMEOUT;
            }
            else{
                // Do nothing
            }
            break;
        default:
            ret = STD_ERROR;
            break;
    }
    return ret;
}

STD_ReturnType RCC_SetClock(RCC_ClockType_t clockType, RCC_Status_t status){
    STD_ReturnType ret = STD_SUCCESS;
    switch (clockType){
        case RCC_CLOCK_SOURCE_HSI:
            RCC->CR.BITS.HSION = status;
            break;
        case RCC_CLOCK_SOURCE_HSE:
            RCC->CR.BITS.HSEON = status;
            break;
        case RCC_CLOCK_SOURCE_PLL:
            RCC->CR.BITS.PLLON = status;
            break;
        default:
            ret = STD_ERROR;
            break;
    }
    return ret;
}


STD_ReturnType RCC_WaitForClockReady(RCC_ClockType_t clockType, uint32_t timeout){
    uint32_t tickStart = 0;
    STD_ReturnType ret = STD_SUCCESS;
    switch (clockType){
        case RCC_CLOCK_SOURCE_HSI:
            while ((RCC->CR.BITS.HSIRDY == 0) && (tickStart < timeout)){
                tickStart++;
            }
            if(timeout == tickStart){
                ret = STD_TIMEOUT;
            }
            else{
                // Do nothing
            }
            break;
        case RCC_CLOCK_SOURCE_HSE:
            while ((RCC->CR.BITS.HSERDY == 0) && (tickStart < timeout)){
                tickStart++;
            }
            if(timeout == tickStart){
                ret = STD_TIMEOUT;
            }
            else{
                // Do nothing
            }
            break;
        case RCC_CLOCK_SOURCE_PLL:
            while ((RCC->CR.BITS.PLLRDY == 0) && (tickStart < timeout)){
                tickStart++;
            }
            if(timeout == tickStart){
                ret = STD_TIMEOUT;
            }
            else{
                // Do nothing
            }
            break;
        default:
            ret = STD_ERROR;
            break;
    }
    return ret;
}

////////////////////// PLL Configurations /////////////////////    
STD_ReturnType RCC_ConfigurePLL(const PLL_CFG_t *pllCfg){
    STD_ReturnType ret = STD_SUCCESS;
    
    if (pllCfg == NULL) {
        ret = STD_ERROR;
    }
    else {
        if (pllCfg->pll_cfg_max_t.pllMax == RCC_PLL_MAX) {
            ret = RCC_SetPLLMaxClock();
        }
        else {
            RCC->PLLCFGR.BITS.PLLM = pllCfg->pll_cfg_custom_t.PLLM;
            RCC->PLLCFGR.BITS.PLLN = pllCfg->pll_cfg_custom_t.PLLN;
            switch (pllCfg->pll_cfg_custom_t.PLLP) {
                case 2:
                    RCC->PLLCFGR.BITS.PLLP = 0b00; // Division by 2
                    break;
                case 4:
                    RCC->PLLCFGR.BITS.PLLP = 0b01; // Division by 4
                    break;
                case 6:
                    RCC->PLLCFGR.BITS.PLLP = 0b10; // Division by 6
                    break;
                case 8:
                    RCC->PLLCFGR.BITS.PLLP = 0b11; // Division by 8
                    break;
                default:
                    ret = STD_ERROR; // Invalid PLLP value
                    break;
            }
            if (ret == STD_SUCCESS) {
                RCC->PLLCFGR.BITS.PLLQ = pllCfg->pll_cfg_custom_t.PLLQ;
            }
            else{
                // Do nothing
            }
        }
    }
    
    return ret;
}

STD_ReturnType RCC_SetPLLClockSource(RCC_ClockType_t source){
    STD_ReturnType ret = STD_SUCCESS;
    switch (source){
        case RCC_CLOCK_SOURCE_HSI:
            RCC->PLLCFGR.BITS.PLLSRC = 0;
            break;
        case RCC_CLOCK_SOURCE_HSE:
            RCC->PLLCFGR.BITS.PLLSRC = 1;
            break;
        default:
            ret = STD_ERROR;
            break;
    }
    return ret;
}

STD_ReturnType RCC_CheckPLLClockSource(RCC_ClockType_t *source){
    STD_ReturnType ret = STD_SUCCESS;
    if(source == NULL){
        ret = STD_ERROR;
        return ret;
    }
    else{
        if (RCC->PLLCFGR.BITS.PLLSRC == 0){
            *source = RCC_CLOCK_SOURCE_HSI;
        }
        else {
            *source = RCC_CLOCK_SOURCE_HSE;
        }
    }

    return ret;
}

STD_ReturnType RCC_SetPLLMaxClock(void){
    STD_ReturnType ret = STD_SUCCESS;
    // Get the current PLL clock source HSI/HSE
    RCC_ClockType_t currentSource = RCC_CLOCK_SOURCE_HSI;
    ret = RCC_CheckPLLClockSource(&currentSource);
    if(ret == STD_SUCCESS){
        if(currentSource == RCC_CLOCK_SOURCE_HSI){
            // Set the PLL to maximum clock settings for HSI (HSI=16MHz)
            RCC->PLLCFGR.BITS.PLLM = 8;  // 16/8 = 2MHz
            RCC->PLLCFGR.BITS.PLLN = 168;// 2*168 = 336MHz
            RCC->PLLCFGR.BITS.PLLP = 1;  // 336/4 = 84MHz
            RCC->PLLCFGR.BITS.PLLQ = 7;  // 336/7 = 48MHz
        }
        else {
            // Set the PLL to maximum clock settings for HSE (HSE=25MHz)
            RCC->PLLCFGR.BITS.PLLM = 25;  // 25/25 = 1 MHz
            RCC->PLLCFGR.BITS.PLLN = 336; // 1 * 336 = 336 MHz
            RCC->PLLCFGR.BITS.PLLP = 1;   // /4 = 84 MHz (00 -> /2, 01 -> /4)
            RCC->PLLCFGR.BITS.PLLQ = 7;   // /7 = 48 MHz (USB)
        }
    }
    else {
        /* STD_ERROR */
    }

    return ret;
}

//////////// Peripherals Configurations (Enable/Disable/Reset) //////////////  
static STD_ReturnType RCC_WaitPeripheralReady(RCC_Peripheral_t peripheral, uint32_t timeout){
    STD_ReturnType ret = STD_SUCCESS;
    uint32_t tickStart = 0;
    uint32_t busID = (peripheral & RCC_BUS_MASK) >> RCC_BUS_OFFSET;  // 1/2/3/4
    
    switch (busID) {
        case RCC_AHB1: 
            while(((RCC->AHB1ENR.REG & peripheral) == 0) && tickStart < timeout){
                tickStart++;
            }
            if(timeout == tickStart){
                ret = STD_TIMEOUT;
            }
            else{
                // Do nothing
            }
            break;
        case RCC_AHB2: 
            while(((RCC->AHB2ENR.REG & peripheral) == 0) && tickStart < timeout){
                tickStart++;
            }
            if(timeout == tickStart){
                ret = STD_TIMEOUT;
            }
            else{
                // Do nothing
            }
            break;
        case RCC_APB1: 
            while(((RCC->APB1ENR.REG & peripheral) == 0) && tickStart < timeout){
                tickStart++;
            }
            if(timeout == tickStart){
                ret = STD_TIMEOUT;
            }
            else{
                // Do nothing
            }
            break;
        case RCC_APB2: 
            while(((RCC->APB2ENR.REG & peripheral) == 0) && tickStart < timeout){
                tickStart++;
            }
            if(timeout == tickStart){
                ret = STD_TIMEOUT;
            }
            else{
                // Do nothing
            }
            break;
        default:
            ret = STD_ERROR;
            break;
    }    

    return ret;
}

// Can send multpile parameters only for the same bus: RCC_GPIOA | RCC_GPIOC
STD_ReturnType RCC_ControlPeripheral(RCC_Peripheral_t peripheral, RCC_Peripheral_Operation_t operation){
    STD_ReturnType ret = STD_SUCCESS;
    uint32_t busID = (peripheral & RCC_BUS_MASK) >> RCC_BUS_OFFSET;  // 1/2/3/4
    
    switch (operation) {
        case RCC_PERIPHERAL_ENABLE:
            switch (busID) {
                case RCC_AHB1: 
                    RCC->AHB1ENR.REG |= peripheral;
                    break;
                case RCC_AHB2: 
                    RCC->AHB2ENR.REG |= peripheral;
                    break;
                case RCC_APB1: 
                    RCC->APB1ENR.REG |= peripheral;
                    break;
                case RCC_APB2: 
                    RCC->APB2ENR.REG |= peripheral;
                    break;
                default:
                    ret = STD_ERROR;
                    break;
            }
            if(ret == STD_SUCCESS){
                ret = RCC_WaitPeripheralReady(peripheral, 50);
            }
            else{
                // Do nothing
            }
            break;
        case RCC_PERIPHERAL_DISABLE:
            switch (busID) {
                case RCC_AHB1: 
                    RCC->AHB1ENR.REG &= ~peripheral;
                    break;
                case RCC_AHB2: 
                    RCC->AHB2ENR.REG &= ~peripheral;
                    break;
                case RCC_APB1: 
                    RCC->APB1ENR.REG &= ~peripheral;
                    break;
                case RCC_APB2: 
                    RCC->APB2ENR.REG &= ~peripheral;
                    break;
                default:
                    ret = STD_ERROR;
                    break;
            }
            if(ret == STD_SUCCESS){
                ret = RCC_WaitPeripheralReady(peripheral, 50);
            }
            else{
                // Do nothing
            }
            break;
        case RCC_PERIPHERAL_RESET:
            switch (busID) {
                case RCC_AHB1: 
                    RCC->AHB1RSTR.REG |= peripheral;
                    RCC->AHB1RSTR.REG &= ~peripheral;
                    break;
                case RCC_AHB2: 
                    RCC->AHB2RSTR.REG |= peripheral;
                    RCC->AHB2RSTR.REG &= ~peripheral;
                    break;
                case RCC_APB1: 
                    RCC->APB1RSTR.REG |= peripheral;
                    RCC->APB1RSTR.REG &= ~peripheral;
                    break;
                case RCC_APB2: 
                    RCC->APB2RSTR.REG |= peripheral;
                    RCC->APB2RSTR.REG &= ~peripheral;
                    break;
                default:
                    ret = STD_ERROR;
                    break;
            }
            if(ret == STD_SUCCESS){
                ret = RCC_WaitPeripheralReady(peripheral, 50);
            }
            else{
                // Do nothing
            }
            break;
        default:
            ret = STD_ERROR;
            break;
    }    

    return ret;
}

////////////////////////// Bus Prescalers ///////////////////////////
STD_ReturnType RCC_SetAHBPrescaler(RCC_AHB_Prescaler_t prescaler){
    STD_ReturnType ret = STD_SUCCESS;
    RCC->CFGR.BITS.HPRE = prescaler;
    return ret;
}
STD_ReturnType RCC_SetAPBPrescaler(RCC_APB_Prescaler_t prescaler, RCC_BusType_t bus){
    STD_ReturnType ret = STD_SUCCESS;
    switch (bus){
        case RCC_APB1:
            RCC->CFGR.BITS.PPRE1 = prescaler;
            break;
        case RCC_APB2:
            RCC->CFGR.BITS.PPRE2 = prescaler;
            break;
        default:
            ret = STD_ERROR;
            break;
    }
    return ret;
}
