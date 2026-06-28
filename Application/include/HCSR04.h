#ifndef HCSR04_H
#define HCSR04_H

#include "STD_TYPES.h"
#include "GPIO_INTERFACE.h"
#include "TIM_INTERFACE.h"

typedef struct {
    GPIO_PORT_t   trigger_port;
    GPIO_PIN_t    trigger_pin;
} HCSR04_Config_t;

/* Initialize the Trigger GPIO for the HC-SR04 */
void HCSR04_Init(const HCSR04_Config_t *cfg);

/* Send a 10 microsecond trigger pulse */
void HCSR04_Trigger(const HCSR04_Config_t *cfg);

#endif // HCSR04_H
