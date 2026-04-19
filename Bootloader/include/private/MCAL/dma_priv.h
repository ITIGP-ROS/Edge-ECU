#ifndef DMA_PRIV_H
#define DMA_PRIV_H

#include "../../../lib/STD_Types.h"

#define DMA1_BASE_ADDRESS 0x40026000
#define DMA2_BASE_ADDRESS 0x40026400

typedef struct{
    volatile uint32_t CR;     // Configuration register
    volatile uint32_t NDTR;   // Number of data register
    volatile uint32_t PAR;    // Peripheral address
    volatile uint32_t M0AR;   // Memory 0 address
    volatile uint32_t M1AR;   // Memory 1 address
    volatile uint32_t FCR;    // FIFO control register
} DMA_Stream_RegDef_t;

typedef struct{
    volatile uint32_t LISR;
    volatile uint32_t HISR;
    volatile uint32_t LIFCR;
    volatile uint32_t HIFCR;
    volatile DMA_Stream_RegDef_t STREAM[8];
} DMA_Registers_t;

#define DMA1 ((DMA_Registers_t*) DMA1_BASE_ADDRESS)
#define DMA2 ((DMA_Registers_t*) DMA2_BASE_ADDRESS)

#endif // DMA_PRIV_H