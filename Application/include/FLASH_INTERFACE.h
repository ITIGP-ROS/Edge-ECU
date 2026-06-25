#ifndef FLASH_INTERFACE_H
#define FLASH_INTERFACE_H

#include "STD_TYPES.h"

/* =======================
 *   Return Status Codes
 * ======================= */
typedef enum
{
    FLASH_OK = 0,                    /* Operation completed successfully       */
    FLASH_ERROR_BUSY,                /* Flash controller is busy               */
    FLASH_ERROR_TIMEOUT,             /* Operation timed out                    */
    FLASH_ERROR_PG_SEQUENCE,         /* Programming sequence error (PGSERR)    */
    FLASH_ERROR_PG_PARALLELISM,      /* Programming parallelism error (PGPERR) */
    FLASH_ERROR_PG_ALIGNMENT,        /* Programming alignment error (PGAERR)   */
    FLASH_ERROR_WRITE_PROTECTION,    /* Write protection error (WRPERR)        */
    FLASH_ERROR_OPERATION,           /* General operation error (OPERR)        */
    FLASH_ERROR_INVALID_SECTOR,      /* Invalid sector number                  */
    FLASH_ERROR_INVALID_ADDRESS,     /* Address out of Flash range             */
    FLASH_ERROR_INVALID_SIZE,        /* Invalid data size for operation        */
    FLASH_ERROR_VERIFY_FAILED,       /* Write verification failed              */
    FLASH_ERROR_NULL_PTR             /* Null pointer passed                    */
} FLASH_Status_t;

/* =======================
 *   Programming Size
 * ======================= */
typedef enum
{
    FLASH_PROGRAM_BYTE = 0,          /* Program 8-bit (byte)                   */
    FLASH_PROGRAM_HALF_WORD,         /* Program 16-bit (half-word)             */
    FLASH_PROGRAM_WORD,              /* Program 32-bit (word)                  */
    FLASH_PROGRAM_DOUBLE_WORD        /* Program 64-bit (double-word)           */
} FLASH_ProgramSize_t;

/* =======================
 *   Sector Numbers
 * ======================= */
typedef enum
{
    FLASH_SECTOR_0 = 0,    /* Sector 0: 16 KB */
    FLASH_SECTOR_1,        /* Sector 1: 16 KB */
    FLASH_SECTOR_2,        /* Sector 2: 16 KB */
    FLASH_SECTOR_3,        /* Sector 3: 16 KB */
    FLASH_SECTOR_4,        /* Sector 4: 64 KB */
    FLASH_SECTOR_5,        /* Sector 5: 128 KB */
    FLASH_SECTOR_TOTAL     /* Total number of sectors */
} FLASH_Sector_t;

/* =======================
 *   Voltage Range (affects max programming size)
 * ======================= */
typedef enum
{
    FLASH_VOLTAGE_1_8V_TO_2_1V = 0,  /* VDD: 1.8V - 2.1V (byte only)          */
    FLASH_VOLTAGE_2_1V_TO_2_7V,      /* VDD: 2.1V - 2.7V (up to half-word)    */
    FLASH_VOLTAGE_2_7V_TO_3_6V       /* VDD: 2.7V - 3.6V (up to double-word)  */
} FLASH_VoltageRange_t;

/* =======================
 *   Core API Functions
 * ======================= */

/**
 * @brief  Unlock the Flash control register for write/erase operations
 * @note   Must be called before any erase or program operation
 * @retval FLASH_Status_t: FLASH_OK or error code
 */
FLASH_Status_t FLASH_Unlock(void);

/**
 * @brief  Lock the Flash control register to prevent accidental writes
 * @note   Should be called after completing Flash operations
 * @retval FLASH_Status_t: FLASH_OK or error code
 */
FLASH_Status_t FLASH_Lock(void);

/**
 * @brief  Erase a specific Flash sector
 * @param  Sector: Sector number to erase (FLASH_SECTOR_0 to FLASH_SECTOR_5)
 * @param  VoltageRange: Operating voltage range (affects erase timing)
 * @retval FLASH_Status_t: FLASH_OK or error code
 * @note   This function blocks until erase completes (~500-1000ms)
 * @warning All data in the sector will be erased (set to 0xFF)
 */
FLASH_Status_t FLASH_EraseSector(FLASH_Sector_t Sector, FLASH_VoltageRange_t VoltageRange);

/**
 * @brief  Erase all Flash sectors (mass erase)
 * @retval FLASH_Status_t: FLASH_OK or error code
 * @note   This function blocks until erase completes
 * @warning ALL Flash memory will be erased - use with extreme caution!
 */
FLASH_Status_t FLASH_MassErase(void);

/**
 * @brief  Program a byte (8-bit) to Flash memory
 * @param  Address: Flash address to write (any address within Flash range)
 * @param  Data: Byte data to write
 * @retval FLASH_Status_t: FLASH_OK or error code
 * @note   Address must be in Flash range, Flash must be erased first
 */
FLASH_Status_t FLASH_ProgramByte(uint32_t Address, uint8_t Data);

/**
 * @brief  Program a half-word (16-bit) to Flash memory
 * @param  Address: Flash address to write (must be half-word aligned)
 * @param  Data: Half-word data to write
 * @retval FLASH_Status_t: FLASH_OK or error code
 * @note   Address must be half-word aligned (even address)
 * @note   Requires VDD >= 2.1V
 */
FLASH_Status_t FLASH_ProgramHalfWord(uint32_t Address, uint16_t Data);

/**
 * @brief  Program a word (32-bit) to Flash memory
 * @param  Address: Flash address to write (must be word aligned)
 * @param  Data: Word data to write
 * @retval FLASH_Status_t: FLASH_OK or error code
 * @note   Address must be word aligned (divisible by 4)
 * @note   Requires VDD >= 2.7V
 */
FLASH_Status_t FLASH_ProgramWord(uint32_t Address, uint32_t Data);

/**
 * @brief  Program a double-word (64-bit) to Flash memory
 * @param  Address: Flash address to write (must be double-word aligned)
 * @param  Data: Pointer to 64-bit data to write
 * @retval FLASH_Status_t: FLASH_OK or error code
 * @note   Address must be double-word aligned (divisible by 8)
 * @note   Requires VDD >= 2.7V
 * @note   Most efficient programming method
 */
FLASH_Status_t FLASH_ProgramDoubleWord(uint32_t Address, uint64_t Data);

/**
 * @brief  Program a buffer of data to Flash memory
 * @param  Address: Starting Flash address (alignment depends on ProgramSize)
 * @param  pData: Pointer to source data buffer
 * @param  Length: Number of bytes to write
 * @param  ProgramSize: Size of each write operation (byte/half-word/word/double-word)
 * @retval FLASH_Status_t: FLASH_OK or error code
 * @note   Length must be compatible with ProgramSize
 * @note   This function handles proper alignment and iteration
 */
FLASH_Status_t FLASH_ProgramBuffer(uint32_t Address, const uint8_t *pData, 
                                    uint32_t Length, FLASH_ProgramSize_t ProgramSize);

/* =======================
 *   Utility Functions
 * ======================= */

/**
 * @brief  Wait until Flash operation completes or timeout occurs
 * @param  Timeout: Timeout value in iterations (use FLASH_TIMEOUT_DEFAULT)
 * @retval FLASH_Status_t: FLASH_OK or FLASH_ERROR_TIMEOUT
 * @note   Internal function, polls BSY flag
 */
FLASH_Status_t FLASH_WaitForLastOperation(uint32_t Timeout);

/**
 * @brief  Get the sector number for a given Flash address
 * @param  Address: Flash address to query
 * @retval FLASH_Sector_t: Sector number, or FLASH_SECTOR_TOTAL if invalid
 */
FLASH_Sector_t FLASH_GetSector(uint32_t Address);

/**
 * @brief  Clear all error flags in Flash status register
 * @retval None
 * @note   Should be called before starting new Flash operations
 */
void FLASH_ClearErrorFlags(void);

/**
 * @brief  Get the last Flash operation error status
 * @retval FLASH_Status_t: Error code from last operation
 * @note   Reads and interprets FLASH_SR error flags
 */
FLASH_Status_t FLASH_GetLastError(void);

/* =======================
 *   Configuration & Status
 * ======================= */

/**
 * @brief  Check if Flash is currently locked
 * @retval uint8_t: 1 if locked, 0 if unlocked
 */
uint8_t FLASH_IsLocked(void);

/**
 * @brief  Check if Flash is currently busy
 * @retval uint8_t: 1 if busy, 0 if idle
 */
uint8_t FLASH_IsBusy(void);



#endif /* FLASH_INTERFACE_H */