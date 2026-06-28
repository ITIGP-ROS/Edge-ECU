/* ADC.c — STM32F401 ADC1 single-channel polling driver */

#include "ADC_REGS.h"
#include "ADC_INTERFACE.h"

/* ============================================================
 *  Internal State
 * ============================================================ */
static uint8_t s_initialized = 0U;
static uint8_t s_channel     = 0U;

/* ============================================================
 *  Helper: write sample time for a channel into SMPR1/SMPR2
 *  Channels 0–9  → SMPR2  (3 bits per channel, starting at bit 0)
 *  Channels 10–18 → SMPR1 (3 bits per channel, starting at bit 0)
 * ============================================================ */
static void adc_set_sample_time(uint8_t ch, ADC_SampleTime_t smp)
{
    uint32_t smp_val = (uint32_t)smp & 0x7U;

    if (ch <= 9U)
    {
        uint32_t shift    = (uint32_t)ch * 3U;
        uint32_t mask     = 0x7UL << shift;
        uint32_t reg      = ADC1->SMPR2.ALL;
        reg              &= ~mask;
        reg              |= smp_val << shift;
        ADC1->SMPR2.ALL   = reg;
    }
    else if (ch <= 18U)
    {
        uint32_t shift    = ((uint32_t)ch - 10U) * 3U;
        uint32_t mask     = 0x7UL << shift;
        uint32_t reg      = ADC1->SMPR1.ALL;
        reg              &= ~mask;
        reg              |= smp_val << shift;
        ADC1->SMPR1.ALL   = reg;
    }
}

/* ============================================================
 *  Public API
 * ============================================================ */

ADC_Error_t ADC_Init(const ADC_Config_t *cfg)
{
    if (cfg == NULL)             { return ADC_ERROR_INVALID_CH; }
    if (cfg->channel > 18U)     { return ADC_ERROR_INVALID_CH; }

    s_channel = cfg->channel;

    /* ---- ADC common prescaler: divide APB2 (84 MHz) by 4 → 21 MHz ---- */
    /* (max ADC clock is 36 MHz for STM32F401)                              */
    ADC_CCR->BITS.ADCPRE = 0x1U;   /* /4 */

    /* ---- Disable ADC before configuring ---- */
    ADC1->CR2.BITS.ADON = 0U;

    /* ---- Resolution ---- */
    ADC1->CR1.BITS.RES = (uint32_t)cfg->resolution & 0x3U;

    /* ---- Single conversion (not continuous), software trigger ---- */
    ADC1->CR2.BITS.CONT    = 0U;
    ADC1->CR2.BITS.EXTEN   = 0U;   /* disable external trigger */
    ADC1->CR2.BITS.ALIGN   = 0U;   /* right-aligned */

    /* ---- Sample time for this channel ---- */
    adc_set_sample_time(cfg->channel, cfg->sample_time);

    /* ---- Regular sequence: length = 1, first (only) channel ---- */
    ADC1->SQR1.BITS.L   = 0U;                   /* 1 conversion */
    ADC1->SQR3.BITS.SQ1 = (uint32_t)cfg->channel & 0x1FU;

    /* ---- Power on ADC ---- */
    ADC1->CR2.BITS.ADON = 1U;

    /* ---- Stabilisation delay: ~10 µs at 84 MHz ≈ 840 cycles ---- */
    for (volatile uint32_t d = 0U; d < 1000U; d++) { __asm("nop"); }

    s_initialized = 1U;
    return ADC_OK;
}

ADC_Error_t ADC_Read(uint16_t *raw_out)
{
    if (raw_out == NULL)  { return ADC_ERROR_INVALID_CH; }
    if (!s_initialized)   { return ADC_ERROR_NOT_INIT;   }

    /* Clear overrun flag if set */
    if (ADC1->SR.BITS.OVR)
    {
        ADC1->SR.BITS.OVR = 0U;
    }

    /* Clear EOC flag (write 0 to clear) before starting */
    ADC1->SR.BITS.EOC = 0U;

    /* Software-trigger a single conversion */
    ADC1->CR2.BITS.SWSTART = 1U;

    /* Wait for End-Of-Conversion (EOC) with a timeout */
    uint32_t timeout = 100000U;
    while (ADC1->SR.BITS.EOC == 0U)
    {
        if (timeout == 0U) { return ADC_ERROR_TIMEOUT; }
        timeout--;
    }

    /* Read data register — this also clears EOC by hardware */
    *raw_out = (uint16_t)(ADC1->DR.BITS.DATA & 0x0FFFU);

    return ADC_OK;
}

uint16_t ADC_LM35_ToTenthsCelsius(uint16_t raw, uint16_t vref_mv)
{
    /* Vout_mV = (raw * vref_mv) / 4095
     * LM35: 10 mV per °C  → Temp_x10 = Vout_mV / 10 * 10 = Vout_mV
     * Wait — Vout_mV / 10 gives °C, so tenths: Vout_mV * 10 / 10 = Vout_mV
     * Simplification:  Temp_x10 = (raw * vref_mv) / 4095
     * Use 32-bit to avoid overflow (max: 4095 * 3300 = ~13.5M fits in uint32) */
    uint32_t vout_mv = ((uint32_t)raw * (uint32_t)vref_mv) / 4095U;
    /* vout_mv in millivolts; LM35: 10 mV = 1°C → vout_mv/10 = °C
     * tenths of °C = vout_mv * 10 / 10 = vout_mv
     * i.e. 1 mV = 0.1°C exactly → return vout_mv directly */
    return (uint16_t)(vout_mv & 0xFFFFU);
}
