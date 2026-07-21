#include "interface/MCAL/rcc.h"
#include "interface/HAL/led.h"
#include "interface/MCAL/gpio.h"
#include "interface/Core/systick.h"
#include "app/bootloader.h"
#include "interface/MCAL/crc.h"
#include "interface/MCAL/flash.h"
#include "app/aes_gcm.h"

RCC_CFG_t rcc_pll = {
    .sysClkSource = RCC_CLOCK_SOURCE_PLL,
    .pllClkSource = RCC_CLOCK_SOURCE_HSE,
    .pllConfig.pll_cfg_max_t = { .pllMax = RCC_PLL_MAX }
};

/* Valid address ranges for STM32F401CC */
#define SRAM_BASE       0x20000000UL
#define SRAM_END        0x20010000UL   /* 64 KB */
#define APP_FLASH_BASE  0x08008000UL   /* Sector 2 start */
#define APP_FLASH_END   0x08040000UL   /* Sector 5 end   */

uint8_t App_IsValid(void){
    uint32_t sp  = *((volatile uint32_t *)(APP_FLASH_BASE));      /* Initial SP  */
    uint32_t pc  = *((volatile uint32_t *)(APP_FLASH_BASE + 4U)); /* Reset_Handler */

    /* 1. SP must point somewhere inside SRAM */
    if(sp < SRAM_BASE || sp > SRAM_END){
        return 0U;
    }
    /* 2. Reset_Handler must be inside the app Flash region */
    if(pc < APP_FLASH_BASE || pc > APP_FLASH_END){
        return 0U;
    }
    /* 3. Cortex-M always runs Thumb — LSB of function pointer must be 1 */
    if((pc & 1U) == 0U){
        return 0U;
    }
    return 1U;
}

void CheckForAppToRun(){
    /* Read the boot flag from reserved RAM (0x2000FFF8) */
    uint32_t flag = *BOOTLOADER_FLAG_ADDR;

    /* Clear the flag immediately so next POR/reset boots into the app */
    *BOOTLOADER_FLAG_ADDR = BOOT_FLAG_CLEAR;

    if(flag == BOOTLOADER_APP_MAGIC){
        /* Application explicitly requested bootloader — stay here */
        return;
    }

    /* Validate the app vector table before jumping */
    if(!App_IsValid()){
        return; /* No valid app, stay in bootloader */
    }

    /* ========= Jump to App ========= */
    uint32_t appSP       = *((volatile uint32_t *)(APP_FLASH_BASE));
    uint32_t MainAppAddr = *((volatile uint32_t *)(APP_FLASH_BASE + 4U));
    mainAppPtr ResetHandler_Address = (mainAppPtr)MainAppAddr;

    /* Set Main Stack Pointer */
    __asm volatile ("MSR msp, %0" : : "r" (appSP) : );

    /* Jump to Application Reset Handler */
    ResetHandler_Address();
}

int main(){
    CheckForAppToRun();

    volatile STD_ReturnType ret = STD_SUCCESS;
    ret = RCC_ConfigureClock(&rcc_pll);
    if(ret == STD_SUCCESS){
        ret = RCC_ControlPeripheral(RCC_GPIOA | RCC_GPIOB | RCC_GPIOC, RCC_PERIPHERAL_ENABLE);
    }

    ret = SYSTICK_Init(SYSTICK_CLOCK_SOURCE_PLL_MAX);
    ret = LED_Init();
    ret = BL_Init(SYSTICK_CLOCK_SOURCE_PLL_MAX);
    ret = LED_SetState(LED_0, LED_HIGH);
    SYSTICK_DelayMS(1000);
    ret = LED_SetState(LED_0, LED_LOW);

    while(1){
        ret = BL_FetchHostCommand();
    }
    
    return 0;
}