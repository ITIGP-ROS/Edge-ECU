#ifndef CRC_PRIV_H
#define CRC_PRIV_H

#include "../../../lib/STD_Types.h"

#define CRC32_BASE_ADDRESS 0x40023000UL

typedef struct {
    volatile uint32_t DR;      // Data Register
    volatile uint32_t IDR;     // Independent Data Register
    volatile uint32_t CR;      // Control Register
} CRC_TypeDef;

#define CRC ((CRC_TypeDef *) CRC32_BASE_ADDRESS)

#endif // CRC_PRIV_H