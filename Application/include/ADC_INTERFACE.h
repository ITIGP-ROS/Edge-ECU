#ifndef ADC_INTERFACE_H_
#define ADC_INTERFACE_H_

#include "STD_TYPES.h"

/* ============================================================
 *  Error Codes
 * ============================================================ */
typedef enum {
    ADC_OK                  = 0x00U,
    ADC_ERROR_INVALID_CH    = 0x01U,   /* Channel number out of range */
    ADC_ERROR_TIMEOUT       = 0x02U,   /* EOC flag did not set in time */
    ADC_ERROR_NOT_INIT      = 0x03U,   /* ADC_Init() not called first */
    ADC_ERROR_OVERRUN       = 0x04U,   /* Overrun flag set */
} ADC_Error_t;

/* ============================================================
 *  Resolution
 * ============================================================ */
typedef enum {
    ADC_RES_12BIT = 0x0U,   /* 0–4095  */
    ADC_RES_10BIT = 0x1U,   /* 0–1023  */
    ADC_RES_8BIT  = 0x2U,   /* 0–255   */
    ADC_RES_6BIT  = 0x3U,   /* 0–63    */
} ADC_Resolution_t;

/* ============================================================
 *  Sample Time
 * ============================================================ */
typedef enum {
    ADC_SAMPLETIME_3    = 0x0U,
    ADC_SAMPLETIME_15   = 0x1U,
    ADC_SAMPLETIME_28   = 0x2U,
    ADC_SAMPLETIME_56   = 0x3U,
    ADC_SAMPLETIME_84   = 0x4U,
    ADC_SAMPLETIME_112  = 0x5U,
    ADC_SAMPLETIME_144  = 0x6U,
    ADC_SAMPLETIME_480  = 0x7U,   /* Best for LM35 (high-impedance) */
} ADC_SampleTime_t;

/* ============================================================
 *  Configuration Structure
 * ============================================================ */
typedef struct {
    uint8_t           channel;      /* ADC channel: 0–18 */
    ADC_Resolution_t  resolution;   /* Conversion resolution */
    ADC_SampleTime_t  sample_time;  /* Sample time for the channel */
} ADC_Config_t;

/* ============================================================
 *  Public API
 * ============================================================ */

/**
 * @brief  Initialise ADC1 for single-channel, software-triggered polling.
 *         - Enables ADC1 clock (caller must have called RCC_EN_CLK_PERIPHERAL)
 *         - Configures resolution and sample time
 *         - Powers up the ADC and waits for stabilisation
 * @param  cfg  Pointer to ADC_Config_t
 * @return ADC_OK or error code
 */
ADC_Error_t ADC_Init(const ADC_Config_t *cfg);

/**
 * @brief  Read a single sample from the configured channel (blocking).
 *         Triggers a software start conversion and waits for EOC.
 * @param  raw_out  Pointer to store the 12-bit raw ADC result
 * @return ADC_OK or error code
 */
ADC_Error_t ADC_Read(uint16_t *raw_out);

/**
 * @brief  Convert a raw ADC reading to temperature in tenths of a degree Celsius.
 *         LM35 formula: Vout = 10 mV/°C
 *         Vout_mV = (raw * 3300) / 4095
 *         Temp_x10 = Vout_mV / 10          → gives tenths of °C
 *         Example: raw=620 → Vout≈499mV → ~49.9°C → returns 499
 * @param  raw        Raw ADC value (0–4095 for 12-bit)
 * @param  vref_mv    Reference voltage in millivolts (typically 3300)
 * @return Temperature in tenths of °C (e.g. 235 = 23.5°C)
 */
uint16_t ADC_LM35_ToTenthsCelsius(uint16_t raw, uint16_t vref_mv);

#endif /* ADC_INTERFACE_H_ */
