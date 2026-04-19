#include "interface/MCAL/flash.h"
#include "private/MCAL/flash_priv.h"

#define FLASH_ERROR_FLAGS ((1 << 4) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8))

STD_ReturnType FLASH_Read(uint32_t address, uint32_t* data, uint32_t length){
    if(data == NULL || length == 0){
        return STD_ERROR;
    }

    // Check if address is valid
    if(address < FLASH_SECTOR_0 || (address + (length * 4)) > FLASH_END){
        return STD_ERROR;
    }
    // Check if address is word-aligned
    if(address % 4 != 0){
        return STD_ERROR;
    }

    // Read data from flash memory
    for(uint32_t memOffset = 0; memOffset < length; memOffset++){
        data[memOffset] = *((volatile uint32_t*)(address + (memOffset * 4)));
    }

    return STD_SUCCESS;
}

STD_ReturnType FLASH_Write(uint32_t address, uint32_t* data, uint32_t length){
    if(data == NULL || length == 0){
        return STD_ERROR;
    }

    // Check if address is valid
    if(address < FLASH_SECTOR_0 || (address + (length * 4)) > FLASH_END){
        return STD_ERROR;
    }
    // Check if address is word-aligned
    if(address % 4 != 0){
        return STD_ERROR;
    }
    // Clear any pending error flags
    FLASH->SR |= FLASH_ERROR_FLAGS;

    // 1. Check Busy Flag
    if(FLASH->SR & (1 << 16)){ // Check if BSY bit is set
        return STD_BUSY;
    }

    // 2. Unlock FLASH CR
    FLASH->KEYR = 0x45670123; // Unlock key 1
    FLASH->KEYR = 0xCDEF89AB; // Unlock key 2

    // 3. Set Programming Size
    FLASH->CR &= ~(0b11 << 8); // Clear PSIZE bits
    FLASH->CR |= (0b10 << 8);  // Set PSIZE to 32-bit

    // 4. Set Programming Bit
    FLASH->CR |= (1 << 0); // Set PG bit

    // 5. Write Data
    for(uint32_t memOffset = 0; memOffset < length; memOffset++){
        // Write 32-bit word to flash memory
        *((volatile uint32_t*)(address + (memOffset * 4))) = data[memOffset];
        // Wait until not busy
        while(FLASH->SR & (1 << 16));
        // Check for Errors
        if (FLASH->SR & FLASH_ERROR_FLAGS) {
            FLASH->CR &= ~(1 << 0);  // Clear PG bit
            FLASH->CR |=  (1 << 31); // Lock FLASH CR 
            return STD_ERROR;
        }
    }
    // Wait until not busy
    while(FLASH->SR & (1 << 16));
    // Check for Errors
    if (FLASH->SR & FLASH_ERROR_FLAGS) {
        FLASH->CR &= ~(1 << 0);  // Clear PG bit
        FLASH->CR |=  (1 << 31); // Lock FLASH CR 
        return STD_ERROR;
    }

    // 6. Clear Programming Bit
    FLASH->CR &= ~(1 << 0); // Clear PG bit

    // 7. Lock FLASH CR
    FLASH->CR |= (1 << 31); // Set LOCK bit

    return STD_SUCCESS;
}

STD_ReturnType FLASH_Erase(FLASH_Sector_Number_t sectorNumber){
    // Check if sector address is valid
    if(sectorNumber < FLASH_SECTOR_NUMBER_0 || (sectorNumber > FLASH_SECTOR_NUMBER_5 && sectorNumber != FLASH_MASS_ERASE)){
        return STD_ERROR;
    }

    // Clear any pending error flags
    FLASH->SR |= FLASH_ERROR_FLAGS;

    // 1. Check Busy Flag
    if(FLASH->SR & (1 << 16)){
        return STD_BUSY;
    }

    // 2. Unlock FLASH CR
    FLASH->KEYR = 0x45670123; // Unlock key 1
    FLASH->KEYR = 0xCDEF89AB; // Unlock key 2

    // Select Erase Type
    if(sectorNumber == FLASH_MASS_ERASE){
        FLASH->CR |= (1 << 15); // Set MER bit for mass erase
    } 
    else {
        FLASH->CR |= (1 << 1);            // Sector erase
        FLASH->CR &= ~(0xF << 3);         // Clear SNB bits
        FLASH->CR |= (sectorNumber << 3); // Set SNB bits for sector erase
    }

    // 3. Set Start Bit
    FLASH->CR |= (1 << 16); // Set STRT bit

    // 4. Wait until not busy
    while(FLASH->SR & (1 << 16));
    if(FLASH->SR & FLASH_ERROR_FLAGS){
        FLASH->CR &= ~((1 << 1) | (1 << 15)); // Clear SER and MER bits
        FLASH->CR |= (1 << 31); // Lock FLASH CR
        return STD_ERROR;
    }

    // 5. Clear SER/MER
    FLASH->CR &= ~((1 << 1) | (1 << 15));

    // 6. Lock FLASH CR
    FLASH->CR |= (1 << 31); // Set LOCK bit

    return STD_SUCCESS;
}

STD_ReturnType FLASH_SetLatency(FLASH_Latency_t latency){
    if(latency > 5){
        return STD_ERROR;
    }

    FLASH->ACR &= ~(0x7);  // Clear latency bits
    FLASH->ACR |= latency; // Set new latency

    return STD_SUCCESS;
}