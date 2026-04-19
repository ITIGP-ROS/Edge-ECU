#ifndef FLASH_PRIV_H
#define FLASH_PRIV_H

#include "../../../lib/STD_Types.h"

#define FLASH_BASE_ADDRESS 0x40023C00

typedef struct{
    volatile uint32_t ACR;
    volatile uint32_t KEYR;
    volatile uint32_t OPTKEYR;
    volatile uint32_t SR;
    volatile uint32_t CR;
    volatile uint32_t OPTCR;
} FLASH_Register_t;

#define FLASH ((volatile FLASH_Register_t*)FLASH_BASE_ADDRESS)

#endif /* FLASH_PRIV_H */