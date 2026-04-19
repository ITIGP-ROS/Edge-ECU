#ifndef MCAL_H
#define MCAL_H

#include "../../../lib/STD_Types.h"

#define FLASH_END 0x0803FFFF

typedef enum{
    FLASH_LATENCY_0WS = 0,  // 0 <= 30 MHz
    FLASH_LATENCY_1WS = 1,  // 30 MHz <= 60 MHz
    FLASH_LATENCY_2WS = 2,  // 60 MHz <= 84 MHz
} FLASH_Latency_t;

typedef enum{
    FLASH_SECTOR_0 = 0x08000000,
    FLASH_SECTOR_1 = 0x08004000,
    FLASH_SECTOR_2 = 0x08008000,
    FLASH_SECTOR_3 = 0x0800C000,
    FLASH_SECTOR_4 = 0x08010000,
    FLASH_SECTOR_5 = 0x08020000,
    FLASH_SECTOR_6 = 0x08040000,    // Not in STM32F401RC
    FLASH_SECTOR_7 = 0x08060000     // Not in STM32F401RC
} FLASH_Sector_t;

typedef enum{
    FLASH_SECTOR_NUMBER_0 = 0,
    FLASH_SECTOR_NUMBER_1,
    FLASH_SECTOR_NUMBER_2,
    FLASH_SECTOR_NUMBER_3,
    FLASH_SECTOR_NUMBER_4,
    FLASH_SECTOR_NUMBER_5,
    FLASH_SECTOR_NUMBER_6,          // Not in STM32F401RC
    FLASH_SECTOR_NUMBER_7,          // Not in STM32F401RC
    FLASH_MASS_ERASE = 0xFF
} FLASH_Sector_Number_t;

STD_ReturnType FLASH_Read(uint32_t address, uint32_t* data, uint32_t length);
STD_ReturnType FLASH_Write(uint32_t address, uint32_t* data, uint32_t length);
STD_ReturnType FLASH_Erase(FLASH_Sector_Number_t sectorNumber);
STD_ReturnType FLASH_SetLatency(FLASH_Latency_t latency);

#endif /* MCAL_H */