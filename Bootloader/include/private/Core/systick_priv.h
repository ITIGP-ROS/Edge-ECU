#ifndef SYSTICK_PRIV_H
#define SYSTICK_PRIV_H

#include "../../../lib/STD_Types.h"

#define SYSTICK_BASE_ADDRESS	0xE000E010UL

typedef struct{
    volatile uint32_t CSR;       // Control and Status Register
    volatile uint32_t RVR;       // Reload Value Register 	 
    volatile uint32_t CVR;       // Current Value Register 	 
    volatile uint32_t CALIB;     // Calibration Value Register
} STK_t;

#define STK     ((STK_t*)SYSTICK_BASE_ADDRESS)

#endif //SYSTICK_PRIV_H