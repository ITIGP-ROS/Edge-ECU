#include "interface/MCAL/rcc.h"
#include "interface/HAL/led.h"
#include "interface/MCAL/gpio.h"
#include "interface/Core/systick.h"
#include "app/bootloader.h"
#include "interface/MCAL/crc.h"
#include "interface/MCAL/flash.h"

RCC_CFG_t rcc_pll = {
    .sysClkSource = RCC_CLOCK_SOURCE_PLL,
    .pllClkSource = RCC_CLOCK_SOURCE_HSE,
    .pllConfig.pll_cfg_max_t = { .pllMax = RCC_PLL_MAX }
};

void CheckForAppToRun(){
    /* USER CODE BEGIN 1 */
	uint32_t flag = 0;
	uint32_t appExists = 0;
    FLASH_Read(FLASH_SECTOR_1, &flag, 1);           // Read flag to know if we want to continue on bootloader or jump if app exists
    FLASH_Read(FLASH_SECTOR_2, &appExists, 1);      // Check if app exists

	if(flag == BOOTLOADER_APP_MAGIC && 0xFFFFFFFF != appExists){
		// ========= Jump to App
		// Value of the main stack pointer of the app
		uint32_t MSP_Value = *((volatile uint32_t *)FLASH_SECTOR_2);
		// Reset Handler function of the app
		uint32_t MainAppAddr = *((volatile uint32_t *)(FLASH_SECTOR_2 + 4));

		// Fetch the reset handler address of the user application
		mainAppPtr ResetHandler_Address = (mainAppPtr)MainAppAddr;

		// Set Main Stack Pointer
		__asm volatile ("MSR msp, %0" : : "r" (MSP_Value) : );

		// Jump to Application Reset Handler
		ResetHandler_Address();
	}
	else {
		// Continue in Bootloader
	}
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
    SYSTICK_DelayMS(2000);
    ret = LED_SetState(LED_0, LED_LOW);

    while(1){
        BL_FetchHostCommand();
    }
    
    return 0;
}