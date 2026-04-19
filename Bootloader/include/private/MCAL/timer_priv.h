#ifndef TIMER_PRIV_H
#define TIMER_PRIV_H

#include "../../../lib/STD_Types.h"

#define TIM2_BASE_ADDRESS 0x40000000
#define TIM3_BASE_ADDRESS 0x40000400
#define TIM4_BASE_ADDRESS 0x40000800
#define TIM5_BASE_ADDRESS 0x40000C00

typedef struct {
    volatile uint32_t CR1;   // Control Register 1
    volatile uint32_t CR2;   // Control Register 2
    volatile uint32_t SMCR;  // Slave Mode Control Register
    volatile uint32_t DIER;  // DMA/Interrupt Enable Register
    volatile uint32_t SR;    // Status Register
    volatile uint32_t EGR;   // Event Generation Register
    volatile uint32_t CCMR1; // Capture/Compare Mode Register 1
    volatile uint32_t CCMR2; // Capture/Compare Mode Register 2
    volatile uint32_t CCER;  // Capture/Compare Enable Register
    volatile uint32_t CNT;   // Counter
    volatile uint32_t PSC;   // Prescaler
    volatile uint32_t ARR;   // Auto-Reload Register
    volatile uint32_t reserved1;
    volatile uint32_t CCR1;  // Capture/Compare Register 1
    volatile uint32_t CCR2;  // Capture/Compare Register 2
    volatile uint32_t CCR3;  // Capture/Compare Register 3
    volatile uint32_t CCR4;  // Capture/Compare Register 4
    volatile uint32_t reserved2;
    volatile uint32_t DCR;   // DMA Control Register
    volatile uint32_t DMAR;  // DMA Address for Full Transfer
    volatile uint32_t OR;    // Option Register
} TIM_TypeDef;

#define TIM2 ((TIM_TypeDef *)TIM2_BASE_ADDRESS)
#define TIM3 ((TIM_TypeDef *)TIM3_BASE_ADDRESS)
#define TIM4 ((TIM_TypeDef *)TIM4_BASE_ADDRESS)
#define TIM5 ((TIM_TypeDef *)TIM5_BASE_ADDRESS)

#endif // TIMER_PRIV_H