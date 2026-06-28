#include "HCSR04.h"

void HCSR04_Init(const HCSR04_Config_t *cfg)
{
    if (cfg == NULL) return;

    /* Initialize the Trigger pin as Output */
    GPIO_CONFIG_t trig;
    trig.Port      = cfg->trigger_port;
    trig.Pin       = cfg->trigger_pin;
    trig.Mode      = GPIO_MODE_OUTPUT;
    trig.Type      = GPIO_OUTPUT_PUSH_PULL;
    trig.Speed     = GPIO_SPEED_VERY_HIGH;
    trig.Pull      = GPIO_NO_PULL;
    trig.Alternate = 0;

    GPIO_INIT(&trig);
    GPIO_WritePin(cfg->trigger_port, cfg->trigger_pin, GPIO_PIN_RESET);
}

void HCSR04_Trigger(const HCSR04_Config_t *cfg)
{
    if (cfg == NULL) return;

    /* Send a 10us HIGH pulse on the trigger pin */
    GPIO_WritePin(cfg->trigger_port, cfg->trigger_pin, GPIO_PIN_SET);

    /* Very basic blocking delay for ~10us at 84MHz */
    /* 84 MHz = 84 cycles per microsecond. For 10 us = 840 cycles. */
    /* A volatile loop of 1000 iterations will be sufficient to exceed 10us at 84MHz */
    for (volatile uint32_t i = 0; i < 1000U; i++)
    {
        /* Just wait */
    }

    GPIO_WritePin(cfg->trigger_port, cfg->trigger_pin, GPIO_PIN_RESET);
}
