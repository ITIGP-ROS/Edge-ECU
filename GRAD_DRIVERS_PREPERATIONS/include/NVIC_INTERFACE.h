#ifndef NVIC_INTERFACE_H
#define NVIC_INTERFACE_H
#include "STD_TYPES.h"
typedef enum
{

    WWDG_IRQ = 0x00,
    PVD_IRQ = 0x0801,
    TAMP_STAMP_IRQ = 0x1002,
    RTC_WKUP_IRQ = 0x1803,
    FLASH_IRQ = 0x2004,
    RCC_IRQ = 0x2805,
    EXTI0_IRQ = 0x3006,
    EXTI1_IRQ = 0x3807,
    EXTI2_IRQ = 0x4008,
    EXTI3_IRQ = 0x4809,
    EXTI4_IRQ = 0x500A,
    DMA1_Stream0_IRQ = 0x580B,
    DMA1_Stream1_IRQ = 0x600C,
    DMA1_Stream2_IRQ = 0x680D,
    DMA1_Stream3_IRQ = 0x700E,
    DMA1_Stream4_IRQ = 0x780F,

    DMA1_Stream5_IRQ = 0x8010,
    DMA1_Stream6_IRQ = 0x8811,
    ADC_IRQ = 0x9012,
    EXTI9_5_IRQ = 0xB817,
    TIM1_BRK_TIM9_IRQ = 0xC018,
    TIM1_UP_TIM10 = 0xC819,
    TIM1_TRG_COM_TIM11 = 0xD01A,
    TIM1_CC = 0xD81B,
    TIM2 = 0xE01C,
    TIM3 = 0xE81D,
    TIM4 = 0xF01E,
    I2C1_EV = 0xF81F,
    I2C1_ER = 0x0120,
    I2C2_EV = 0x0921,
    I2C2_ER = 0x1122,
    SPI1 = 0x1923,
    SPI2 = 0x2124,
    USART1 = 0x2925,
    USART2 = 0x3126,
    EXTI15_10 = 0x4128,
    EXTI17_RTC_Alarm = 0x4929, // 169
    EXTI18_OTG_FS_WKUP = 0x512A,
    DMA1_Stream7 = 0x792F,
    SDIO = 0x8931, // 177
    TIM5 = 0x9132,
    SPI3 = 0x9933,
    DMA2_Stream0 = 0xC138, // 184
    DMA2_Stream1 = 0xC939,
    DMA2_Stream2 = 0xD13A,
    DMA2_Stream3 = 0xD93B,
    DMA2_Stream4 = 0xE13C,
    OTG_FS = 0x1A43, // 451
    DMA2_Stream5 = 0x2244,
    DMA2_Stream6 = 0x2A45,
    DMA2_Stream7 = 0x3246,
    USART6 = 0x3A47,
    I2C3_EV = 0x4248,
    I2C3_ER = 0x4A49,
    FPU = 0x8A51,
    SPI4 = 0xA254,
} IRQn_t;

typedef enum {
    NVIC_ERROR_OK                     = 0x00U, /* N-09: explicit value assigned — MISRA Rule 8.12 */
    NVIC_ERROR_INVALID_IRQ_NUM        = 0x01U, /* N-09: explicit value assigned — MISRA Rule 8.12 */
    NVIC_ERROR_INVALID_REG_INDEX      = 0x02U, /* N-09: explicit value assigned — MISRA Rule 8.12 */
    NVIC_ERROR_INVALID_BIT_POS        = 0x03U, /* N-09: explicit value assigned — MISRA Rule 8.12 */
    NVIC_ERROR_INCONSISTENT_ENCODING  = 0x04U, /* N-09: explicit value assigned — MISRA Rule 8.12 */
    NVIC_ERROR_INVALID_PRIORITY       = 0x05U, /* N-09: explicit value assigned — MISRA Rule 8.12 */
    NVIC_ERROR_INVALID_PRIORITY_GROUP = 0x06U, /* N-09: explicit value assigned — MISRA Rule 8.12 */
    NVIC_ERROR_INVALID_PARAM          = 0x07U  /* N-09: new entry for NULL output-pointer check (N-14) */
} NVIC_ERROR_t;

NVIC_ERROR_t NVIC_EnableIRQ(IRQn_t IRQn);
NVIC_ERROR_t NVIC_DisableIRQ(IRQn_t IRQn);
NVIC_ERROR_t NVIC_SetPendingIRQ(IRQn_t IRQn);
NVIC_ERROR_t NVIC_GetPendingIRQ(IRQn_t IRQn, uint8_t *pending);   /* N-14: return type changed to NVIC_ERROR_t; output pointer added — error was indistinguishable from valid "pending=1" */
NVIC_ERROR_t NVIC_ClearPendingIRQ(IRQn_t IRQn);
NVIC_ERROR_t NVIC_GetActive(IRQn_t IRQn, uint8_t *active);        /* N-14: return type changed to NVIC_ERROR_t; output pointer added — error was indistinguishable from valid "active=1" */
NVIC_ERROR_t NVIC_SetPriority(IRQn_t IRQn, uint32_t priority);
NVIC_ERROR_t NVIC_GetPriority(IRQn_t IRQn, uint8_t *priority);    /* N-14: return type changed to NVIC_ERROR_t; output pointer added — error was indistinguishable from valid priority value */
NVIC_ERROR_t NVIC_SetPriorityGrouping(uint32_t priority_grouping);
void NVIC_SystemReset(void) __attribute__((noreturn));             /* N-15: return type void (noreturn function returning error is meaningless); noreturn attribute added */


#endif
