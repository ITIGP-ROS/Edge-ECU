#include "private/Core/systick_priv.h"
#include "configuration/Core/systick_conf.h"
#include "interface/Core/systick.h"

static STD_ReturnType delay(uint32_t delayTicks);

static volatile FuncPtr SysTickHandler_CallBack = NULL;

volatile uint32_t SYSTICK_mode = SYSTICK_SINGLE_INTERVAL_MODE;
static uint32_t sysClockSourceKHz = 0;
extern volatile uint8_t schedStartFlag;

STD_ReturnType SYSTICK_Init(SYSTICK_ClockSource_t clockSource){
	STD_ReturnType ret = STD_SUCCESS;

	SYSTICK_Stop();
	// Set Clock Source
	STK->CSR |= (1 << SYSTICK_CSR_CLKSOURCE_BIT_POS);
	if(clockSource == SYSTICK_CLOCK_SOURCE_HSI){
		sysClockSourceKHz = 16000;
	}
	else if(clockSource == SYSTICK_CLOCK_SOURCE_HSE){
		sysClockSourceKHz = 25000;
	}
	else if(clockSource == SYSTICK_CLOCK_SOURCE_PLL_MAX){
		// Assuming PLL is configured to 84MHz
		sysClockSourceKHz = 84000;
	}
	else{
		ret = STD_ERROR;
	}
	#if (SYSTICK_CLOCK_SOURCE == SYSTICK_CLOCK_SOURCE_PROCESSOR_DIV1)
		// Nothing to do, already set
	#elif (SYSTICK_CLOCK_SOURCE == SYSTICK_CLOCK_SOURCE_PROCESSOR_DIV8)
		sysClockSourceKHz /= 8;
		STK->CSR &= ~(1 << SYSTICK_CSR_CLKSOURCE_BIT_POS);
	#else
		#error "Invalid Clock Source"
	#endif
	STK->CSR &= ~(1 << SYSTICK_CSR_TICKINT_BIT_POS);	// Disable Exception Request

	return ret;
}

STD_ReturnType SYSTICK_Stop(void){
	STD_ReturnType ret = STD_SUCCESS;

	STK->CSR &= ~(1 << SYSTICK_CSR_ENABLE_BIT_POS);		// Disable
	STK->RVR = 0;										// Clear Reload Value
	STK->CVR = 0;										// Clear Current Value
	STK->CSR &= ~(1 << SYSTICK_CSR_TICKINT_BIT_POS);	// Disable Exception Request

	return ret;
}

STD_ReturnType SYSTICK_Enable(void){
	STD_ReturnType ret = STD_SUCCESS;

	STK->CSR |= (1 << SYSTICK_CSR_ENABLE_BIT_POS);

	return ret;
}

STD_ReturnType SYSTICK_Disable(void){
	STD_ReturnType ret = STD_SUCCESS;

	STK->CSR &= ~(1 << SYSTICK_CSR_ENABLE_BIT_POS);

	return ret;
}

STD_ReturnType SYSTICK_DelayMS(uint16_t delayMillieSec){
	STD_ReturnType ret = STD_SUCCESS;

	uint32_t fullDelay = (delayMillieSec * sysClockSourceKHz) - 1;
	uint32_t reqReloadVal = 0;
	while(fullDelay > SYSTICK_MAX_TICKS){
		reqReloadVal = SYSTICK_MAX_TICKS;
		fullDelay -= SYSTICK_MAX_TICKS;
		ret = delay(reqReloadVal);
	}
	ret = delay(fullDelay);

	return ret;
}

STD_ReturnType SYSTICK_DelayUS(uint32_t delayMicroSec){
    STD_ReturnType ret = STD_SUCCESS;

    uint32_t fullDelay = (delayMicroSec * (sysClockSourceKHz / 1000)) - 1;
	uint32_t reqReloadVal = 0;
	while(fullDelay > SYSTICK_MAX_TICKS){
		reqReloadVal = SYSTICK_MAX_TICKS;
		fullDelay -= SYSTICK_MAX_TICKS;
		ret = delay(reqReloadVal);
	}
	ret = delay(fullDelay);

    return ret;
}

static STD_ReturnType delay(uint32_t delayTicks){
	STD_ReturnType ret = STD_SUCCESS;

	STK->RVR = delayTicks;
	STK->CVR = 0;
	STK->CSR |= (1 << SYSTICK_CSR_ENABLE_BIT_POS);
	while(!((STK->CSR >> SYSTICK_CSR_COUNTFLAG_BIT_POS) & 1U));
	//ret = SYSTICK_Stop();

	return ret;
}

STD_ReturnType SYSTICK_SingleInterval(uint16_t delayMillieSec, FuncPtr SysTickHandler_CB){
	STD_ReturnType ret = STD_SUCCESS;

	uint32_t reqReloadVal = (delayMillieSec * sysClockSourceKHz) - 1;
	if(reqReloadVal > SYSTICK_MAX_TICKS){
		ret = STD_ERROR;
	}
	else{
		STK->RVR = reqReloadVal;
		SysTickHandler_CallBack = SysTickHandler_CB;
		STK->CSR |= (1 << SYSTICK_CSR_TICKINT_BIT_POS);
		SYSTICK_mode = SYSTICK_SINGLE_INTERVAL_MODE;		
		STK->CSR |= (1 << SYSTICK_CSR_ENABLE_BIT_POS);
	}

	return ret;
}

STD_ReturnType SYSTICK_PeriodicInterval(uint16_t delayMillieSec, FuncPtr SysTickHandler_CB){
	STD_ReturnType ret = STD_SUCCESS;

	uint32_t reqReloadVal = (delayMillieSec * sysClockSourceKHz) - 1;
	if(reqReloadVal > SYSTICK_MAX_TICKS){
		ret = STD_ERROR;
	}
	else{
		STK->RVR = reqReloadVal;
		SysTickHandler_CallBack = SysTickHandler_CB;
		STK->CSR |= (1 << SYSTICK_CSR_TICKINT_BIT_POS);
		SYSTICK_mode = SYSTICK_PERIODIC_INTERVAL_MODE;
		STK->CSR |= (1 << SYSTICK_CSR_ENABLE_BIT_POS);
	}

	return ret;
}

STD_ReturnType SYSTICK_GetRemainingTicks(uint32_t* remTicks){
	STD_ReturnType ret = STD_SUCCESS;

	if(remTicks == NULL){
		ret = STD_ERROR;
	}
	else{
		*remTicks = STK->CVR;
	}

	return ret;
}

STD_ReturnType SYSTICK_GetElapsedTicks(uint32_t* elapsedTicks){
	STD_ReturnType ret = STD_SUCCESS;

	if(elapsedTicks == NULL){
		ret = STD_ERROR;
	}
	else{
		*elapsedTicks = STK->RVR - STK->CVR;
	}

	return ret;
}

