#ifndef SYSTICK_H
#define SYSTICK_H

#include "../../../lib/STD_Types.h"

typedef void (*FuncPtr)(void);

#define SYSTICK_CLOCK_SOURCE_PROCESSOR_DIV1		    0
#define SYSTICK_CLOCK_SOURCE_PROCESSOR_DIV8		    1

#define SYSTICK_CSR_COUNTFLAG_BIT_POS		        16U
#define SYSTICK_CSR_CLKSOURCE_BIT_POS		        2U
#define SYSTICK_CSR_TICKINT_BIT_POS			        1U
#define SYSTICK_CSR_ENABLE_BIT_POS			        0U

#define SYSTICK_CALIB_NOREF_BIT_POS			        31U
#define SYSTICK_CALIB_SKEW_BIT_POS			        30U

#define SYSTICK_SINGLE_INTERVAL_MODE				0
#define SYSTICK_PERIODIC_INTERVAL_MODE				1

typedef enum {
    SYSTICK_CLOCK_SOURCE_HSI = 0,
    SYSTICK_CLOCK_SOURCE_HSE,
    SYSTICK_CLOCK_SOURCE_PLL_MAX
} SYSTICK_ClockSource_t;

STD_ReturnType SYSTICK_Init(SYSTICK_ClockSource_t clockSource);
STD_ReturnType SYSTICK_Stop(void);
STD_ReturnType SYSTICK_Enable(void);
STD_ReturnType SYSTICK_Disable(void);
STD_ReturnType SYSTICK_DelayMS(uint16_t delayMillieSec);
STD_ReturnType SYSTICK_DelayUS(uint32_t delayMicroSec);
STD_ReturnType SYSTICK_SingleInterval(uint16_t delayMillieSec, FuncPtr SysTickHandler_CB);
STD_ReturnType SYSTICK_PeriodicInterval(uint16_t delayMillieSec, FuncPtr SysTickKHandler_CB);
STD_ReturnType SYSTICK_GetRemainingTicks(uint32_t* remTicks);
STD_ReturnType SYSTICK_GetElapsedTicks(uint32_t* elapsedTicks);

#endif //SYSTICK_H