/* FLASH.c */

#include "FLASH_REGS.h"
#include "FLASH_INTERFACE.h"

/* =======================
 *   Helper Functions
 * ======================= */

/**
 * @brief  Clear all error flags in Flash status register
 * @retval None
 */
void FLASH_ClearErrorFlags(void)
{
    /* Write 1 to clear error flags (w1c - write 1 to clear mechanism) */
    FLASH->SR.ALL = FLASH_SR_ERROR_MASK;
}

/**
 * @brief  Check if Flash is currently busy
 * @retval uint8_t: 1 if busy, 0 if idle
 */
uint8_t FLASH_IsBusy(void)
{
    return (FLASH->SR.BITS.BSY == 1) ? 1 : 0;
}

/**
 * @brief  Check if Flash is currently locked
 * @retval uint8_t: 1 if locked, 0 if unlocked
 */
uint8_t FLASH_IsLocked(void)
{
    return (FLASH->CR.BITS.LOCK == 1) ? 1 : 0;
}

/**
 * @brief  Get the last Flash operation error status
 * @retval FLASH_Status_t: Error code based on SR flags
 * @note   Checks error flags in priority order (specific to general)
 */
FLASH_Status_t FLASH_GetLastError(void)
{
    FLASH_Status_t status = FLASH_OK;
    
    /* Read status register once to avoid multiple volatile accesses */
    uint32_t sr_value = FLASH->SR.ALL;
    
    /* Check error flags in priority order - most specific first */
    
    if (sr_value & FLASH_SR_PGAERR)
    {
        /* Programming alignment error - address not properly aligned */
        status = FLASH_ERROR_PG_ALIGNMENT;
    }
    else if (sr_value & FLASH_SR_PGPERR)
    {
        /* Programming parallelism error - PSIZE doesn't match actual write size */
        status = FLASH_ERROR_PG_PARALLELISM;
    }
    else if (sr_value & FLASH_SR_PGSERR)
    {
        /* Programming sequence error - wrong operation order or conflicting bits in CR */
        status = FLASH_ERROR_PG_SEQUENCE;
    }
    else if (sr_value & FLASH_SR_WRPERR)
    {
        /* Write protection error - attempted to write to protected sector */
        status = FLASH_ERROR_WRITE_PROTECTION;
    }
    else if (sr_value & FLASH_SR_OPERR)
    {
        /* General operation error */
        status = FLASH_ERROR_OPERATION;
    }
    else
    {
        /* No errors detected */
        status = FLASH_OK;
    }
    
    return status;
}

/**
 * @brief  Wait until Flash operation completes or timeout occurs
 * @param  Timeout: Timeout value in iterations
 * @retval FLASH_Status_t: FLASH_OK, error code, or FLASH_ERROR_TIMEOUT
 * @note   This function:
 *         1. Polls BSY flag until it clears or timeout
 *         2. Checks for errors after BSY clears
 *         3. Returns appropriate status
 */
FLASH_Status_t FLASH_WaitForLastOperation(uint32_t Timeout)
{
    FLASH_Status_t status = FLASH_OK;
    uint32_t timeout_counter = Timeout;
    
    /* Wait while Flash is busy */
    while (FLASH_IsBusy())
    {
        /* Decrement timeout counter */
        timeout_counter--;
        
        /* Check if timeout occurred */
        if (timeout_counter == 0)
        {
            return FLASH_ERROR_TIMEOUT;
        }
    }
    
    /* BSY flag cleared - operation completed */
    /* Now check if operation succeeded or failed */
    status = FLASH_GetLastError();
    
    /* If error occurred, clear the error flags for next operation */
    if (status != FLASH_OK)
    {
        FLASH_ClearErrorFlags();
    }
    
    return status;
}

/**
 * @brief  Unlock the Flash control register for write/erase operations
 * @retval FLASH_Status_t: FLASH_OK or FLASH_ERROR_OPERATION
 * @note   Must be called before any erase or program operation
 *         Flash controller is locked by default after reset
 */
FLASH_Status_t FLASH_Unlock(void)
{
    /* Check if Flash is already unlocked */
    if (!FLASH_IsLocked())
    {
        /* Already unlocked, nothing to do */
        return FLASH_OK;
    }
    
    /* Write the unlock sequence to KEYR register */
    FLASH->KEYR.ALL = FLASH_KEY1;  /* First key: 0x45670123 */
    FLASH->KEYR.ALL = FLASH_KEY2;  /* Second key: 0xCDEF89AB */
    
    /* Verify that unlock succeeded by checking LOCK bit in CR */
    if (FLASH_IsLocked())
    {
        /* Unlock failed - LOCK bit still set */
        /* This shouldn't happen with correct keys, indicates hardware issue */
        return FLASH_ERROR_OPERATION;
    }
    
    return FLASH_OK;
}

/**
 * @brief  Lock the Flash control register to prevent accidental writes
 * @retval FLASH_Status_t: FLASH_OK or FLASH_ERROR_OPERATION
 * @note   Should be called after completing Flash operations
 *         Locking prevents accidental Flash corruption
 */
FLASH_Status_t FLASH_Lock(void)
{
    /* Check if Flash is already locked */
    if (FLASH_IsLocked())
    {
        /* Already locked, nothing to do */
        return FLASH_OK;
    }
    
    /* Set LOCK bit in CR register to lock Flash */
    FLASH->CR.BITS.LOCK = 1;
    
    /* Verify that lock succeeded */
    if (!FLASH_IsLocked())
    {
        /* Lock failed - LOCK bit still clear */
        /* This shouldn't happen, indicates hardware issue */
        return FLASH_ERROR_OPERATION;
    }
    
    return FLASH_OK;
}

/**
 * @brief  Erase a specific Flash sector
 * @param  Sector: Sector number to erase (FLASH_SECTOR_0 to FLASH_SECTOR_5)
 * @param  VoltageRange: Operating voltage range (affects erase parallelism)
 * @retval FLASH_Status_t: FLASH_OK or error code
 * @note   This function blocks until erase completes (~500-1000ms depending on sector size)
 * @warning All data in the sector will be erased (set to 0xFF)
 *          Flash must be unlocked before calling this function
 */
FLASH_Status_t FLASH_EraseSector(FLASH_Sector_t Sector, FLASH_VoltageRange_t VoltageRange)
{
    FLASH_Status_t status = FLASH_OK;
    
    /* Input validation: Check sector number is valid */
    if (Sector >= FLASH_SECTOR_TOTAL)
    {
        return FLASH_ERROR_INVALID_SECTOR;
    }
    
    /* Wait for any ongoing Flash operation to complete */
    status = FLASH_WaitForLastOperation(FLASH_TIMEOUT_DEFAULT);
    if (status != FLASH_OK)
    {
        return status;
    }
    
    /* Clear all error flags from previous operations */
    FLASH_ClearErrorFlags();
    
    /* Configure Flash CR register for sector erase:
     * 1. Clear previous operation bits (PG, SER, MER)
     * 2. Set SER (Sector Erase) bit
     * 3. Set SNB (Sector Number) bits
     * 4. Set PSIZE (Program Size) based on voltage range
     */
    
    /* Clear operation control bits first */
    FLASH->CR.BITS.PG = 0;   /* Clear programming bit */
    FLASH->CR.BITS.MER = 0;  /* Clear mass erase bit */
    
    /* Set sector erase mode */
    FLASH->CR.BITS.SER = 1;
    
    /* Set the sector number to erase */
    FLASH->CR.BITS.SNB = (uint32_t)Sector;
    
    /* Set PSIZE based on voltage range */
    switch (VoltageRange)
    {
        case FLASH_VOLTAGE_1_8V_TO_2_1V:
            FLASH->CR.BITS.PSIZE = FLASH_PSIZE_BYTE;
            break;
            
        case FLASH_VOLTAGE_2_1V_TO_2_7V:
            FLASH->CR.BITS.PSIZE = FLASH_PSIZE_HALF_WORD;
            break;
            
        case FLASH_VOLTAGE_2_7V_TO_3_6V:
            FLASH->CR.BITS.PSIZE = FLASH_PSIZE_WORD;  /* Fastest erase for STM32F401 is 32-bit word */
            break;
            
        default:
            /* Invalid voltage range - use safest option */
            FLASH->CR.BITS.PSIZE = FLASH_PSIZE_BYTE;
            break;
    }
    
    /* Start the erase operation */
    FLASH->CR.BITS.STRT = 1;
    
    /* Wait for the erase operation to complete */
    /* Use longer timeout for erase operations (they take ~500-1000ms) */
    status = FLASH_WaitForLastOperation(FLASH_TIMEOUT_ERASE);
    
    /* Clear the sector erase bit and sector number (cleanup) */
    FLASH->CR.BITS.SER = 0;
    FLASH->CR.BITS.SNB = 0;
    
    return status;
}


/**
 * @brief  Erase all Flash sectors (mass erase)
 * @retval FLASH_Status_t: FLASH_OK or error code
 * @note   This function blocks until erase completes (several seconds)
 * @warning ALL Flash memory will be erased - use with extreme caution!
 *          This will erase your running program if executing from Flash
 *          Flash must be unlocked before calling this function
 */
FLASH_Status_t FLASH_MassErase(void)
{
    FLASH_Status_t status = FLASH_OK;
    
    /* Wait for any ongoing Flash operation to complete */
    status = FLASH_WaitForLastOperation(FLASH_TIMEOUT_DEFAULT);
    if (status != FLASH_OK)
    {
        return status;
    }
    
    /* Clear all error flags from previous operations */
    FLASH_ClearErrorFlags();
    
    /* Configure Flash CR register for mass erase:
     * 1. Clear previous operation bits
     * 2. Set MER (Mass Erase) bit
     * 3. Set PSIZE for maximum speed (double-word at 2.7V+)
     */
    
    /* Clear operation control bits */
    FLASH->CR.BITS.PG = 0;   /* Clear programming bit */
    FLASH->CR.BITS.SER = 0;  /* Clear sector erase bit */
    
    /* Set mass erase mode */
    FLASH->CR.BITS.MER = 1;
    
    /* Set PSIZE for fastest erase (assumes VDD >= 2.7V, which is word for F401) */
    FLASH->CR.BITS.PSIZE = FLASH_PSIZE_WORD;
    
    /* Start the mass erase operation */
    FLASH->CR.BITS.STRT = 1;
    
    /* Wait for the mass erase to complete */
    /* Mass erase takes much longer - multiply timeout */
    status = FLASH_WaitForLastOperation(FLASH_TIMEOUT_ERASE * 10);
    
    /* Clear the mass erase bit (cleanup) */
    FLASH->CR.BITS.MER = 0;
    
    return status;
}



/**
 * @brief  Program a byte (8-bit) to Flash memory
 * @param  Address: Flash address to write (any address within Flash range)
 * @param  Data: Byte data to write
 * @retval FLASH_Status_t: FLASH_OK or error code
 * @note   Flash must be unlocked and erased before programming
 */
FLASH_Status_t FLASH_ProgramByte(uint32_t Address, uint8_t Data)
{
    FLASH_Status_t status = FLASH_OK;
    
    /* Input validation: Check address is within Flash range */
    if (Address < FLASH_START_ADDRESS || Address > FLASH_END_ADDRESS)
    {
        return FLASH_ERROR_INVALID_ADDRESS;
    }
    
    /* Wait for any ongoing Flash operation to complete */
    status = FLASH_WaitForLastOperation(FLASH_TIMEOUT_DEFAULT);
    if (status != FLASH_OK)
    {
        return status;
    }
    
    /* Clear all error flags from previous operations */
    FLASH_ClearErrorFlags();
    
    /* Configure for byte programming:
     * 1. Set PG (Programming) bit
     * 2. Set PSIZE to byte (x8)
     */
    FLASH->CR.BITS.PG = 1;
    FLASH->CR.BITS.PSIZE = FLASH_PSIZE_BYTE;
    
    /* Perform the write - this triggers the Flash programming operation */
    *(volatile uint8_t *)Address = Data;
    
    /* Wait for the programming operation to complete */
    status = FLASH_WaitForLastOperation(FLASH_TIMEOUT_DEFAULT);
    
    /* Clear the programming bit */
    FLASH->CR.BITS.PG = 0;
    
    return status;
}


/**
 * @brief  Program a half-word (16-bit) to Flash memory
 * @param  Address: Flash address to write (must be half-word aligned - even)
 * @param  Data: Half-word data to write
 * @retval FLASH_Status_t: FLASH_OK or error code
 * @note   Address must be half-word aligned (even address)
 *         Requires VDD >= 2.1V
 *         Flash must be unlocked and erased before programming
 */
FLASH_Status_t FLASH_ProgramHalfWord(uint32_t Address, uint16_t Data)
{
    FLASH_Status_t status = FLASH_OK;
    
    /* Input validation: Check address is within Flash range */
    if (Address < FLASH_START_ADDRESS || Address > FLASH_END_ADDRESS)
    {
        return FLASH_ERROR_INVALID_ADDRESS;
    }
    
    /* Check half-word alignment (address must be even) */
    if ((Address & 0x1) != 0)
    {
        return FLASH_ERROR_PG_ALIGNMENT;
    }
    
    /* Wait for any ongoing Flash operation to complete */
    status = FLASH_WaitForLastOperation(FLASH_TIMEOUT_DEFAULT);
    if (status != FLASH_OK)
    {
        return status;
    }
    
    /* Clear all error flags from previous operations */
    FLASH_ClearErrorFlags();
    
    /* Configure for half-word programming:
     * 1. Set PG (Programming) bit
     * 2. Set PSIZE to half-word (x16)
     */
    FLASH->CR.BITS.PG = 1;
    FLASH->CR.BITS.PSIZE = FLASH_PSIZE_HALF_WORD;
    
    /* Perform the write - this triggers the Flash programming operation */
    *(volatile uint16_t *)Address = Data;
    
    /* Wait for the programming operation to complete */
    status = FLASH_WaitForLastOperation(FLASH_TIMEOUT_DEFAULT);
    
    /* Clear the programming bit */
    FLASH->CR.BITS.PG = 0;
    
    return status;
}


/**
 * @brief  Program a word (32-bit) to Flash memory
 * @param  Address: Flash address to write (must be word aligned - divisible by 4)
 * @param  Data: Word data to write
 * @retval FLASH_Status_t: FLASH_OK or error code
 * @note   Address must be word aligned (divisible by 4)
 *         Requires VDD >= 2.7V
 *         Flash must be unlocked and erased before programming
 */
FLASH_Status_t FLASH_ProgramWord(uint32_t Address, uint32_t Data)
{
    FLASH_Status_t status = FLASH_OK;
    
    /* Input validation: Check address is within Flash range */
    if (Address < FLASH_START_ADDRESS || Address > FLASH_END_ADDRESS)
    {
        return FLASH_ERROR_INVALID_ADDRESS;
    }
    
    /* Check word alignment (address must be divisible by 4) */
    if ((Address & 0x3) != 0)
    {
        return FLASH_ERROR_PG_ALIGNMENT;
    }
    
    /* Wait for any ongoing Flash operation to complete */
    status = FLASH_WaitForLastOperation(FLASH_TIMEOUT_DEFAULT);
    if (status != FLASH_OK)
    {
        return status;
    }
    
    /* Clear all error flags from previous operations */
    FLASH_ClearErrorFlags();
    
    /* Configure for word programming:
     * 1. Set PG (Programming) bit
     * 2. Set PSIZE to word (x32)
     */
    FLASH->CR.BITS.PG = 1;
    FLASH->CR.BITS.PSIZE = FLASH_PSIZE_WORD;
    
    /* Perform the write - this triggers the Flash programming operation */
    *(volatile uint32_t *)Address = Data;
    
    /* Wait for the programming operation to complete */
    status = FLASH_WaitForLastOperation(FLASH_TIMEOUT_DEFAULT);
    
    /* Clear the programming bit */
    FLASH->CR.BITS.PG = 0;
    
    return status;
}


/**
 * @brief  Program a double-word (64-bit) to Flash memory
 * @param  Address: Flash address to write (must be double-word aligned - divisible by 8)
 * @param  Data: Double-word data to write
 * @retval FLASH_Status_t: FLASH_OK or error code
 * @note   Address must be double-word aligned (divisible by 8)
 *         Requires VDD >= 2.7V
 *         Flash must be unlocked and erased before programming
 *         This is the fastest programming method
 */
FLASH_Status_t FLASH_ProgramDoubleWord(uint32_t Address, uint64_t Data)
{
    FLASH_Status_t status = FLASH_OK;
    
    /* Input validation: Check address is within Flash range */
    if (Address < FLASH_START_ADDRESS || Address > FLASH_END_ADDRESS)
    {
        return FLASH_ERROR_INVALID_ADDRESS;
    }
    
    /* Check double-word alignment (address must be divisible by 8) */
    if ((Address & 0x7) != 0)
    {
        return FLASH_ERROR_PG_ALIGNMENT;
    }
    
    /* Wait for any ongoing Flash operation to complete */
    status = FLASH_WaitForLastOperation(FLASH_TIMEOUT_DEFAULT);
    if (status != FLASH_OK)
    {
        return status;
    }
    
    /* Clear all error flags from previous operations */
    FLASH_ClearErrorFlags();
    
    /* Configure for double-word programming:
     * 1. Set PG (Programming) bit
     * 2. Set PSIZE to double-word (x64)
     */
    FLASH->CR.BITS.PG = 1;
    FLASH->CR.BITS.PSIZE = FLASH_PSIZE_DOUBLE_WORD;
    
    /* Perform the write - this triggers the Flash programming operation */
    *(volatile uint64_t *)Address = Data;
    
    /* Wait for the programming operation to complete */
    status = FLASH_WaitForLastOperation(FLASH_TIMEOUT_DEFAULT);
    
    /* Clear the programming bit */
    FLASH->CR.BITS.PG = 0;
    
    return status;
}


/**
 * @brief  Get the sector number for a given Flash address
 * @param  Address: Flash address to query
 * @retval FLASH_Sector_t: Sector number (0-5), or FLASH_SECTOR_TOTAL if invalid
 * @note   Returns FLASH_SECTOR_TOTAL for addresses outside Flash range
 */
FLASH_Sector_t FLASH_GetSector(uint32_t Address)
{
    FLASH_Sector_t sector = FLASH_SECTOR_TOTAL;  /* Default: invalid */
    
    /* Check which sector the address falls into */
    if (Address < FLASH_SECTOR_1_BASE)
    {
        /* Address is in sector 0 range */
        sector = FLASH_SECTOR_0;
    }
    else if (Address < FLASH_SECTOR_2_BASE)
    {
        /* Address is in sector 1 range */
        sector = FLASH_SECTOR_1;
    }
    else if (Address < FLASH_SECTOR_3_BASE)
    {
        /* Address is in sector 2 range */
        sector = FLASH_SECTOR_2;
    }
    else if (Address < FLASH_SECTOR_4_BASE)
    {
        /* Address is in sector 3 range */
        sector = FLASH_SECTOR_3;
    }
    else if (Address < FLASH_SECTOR_5_BASE)
    {
        /* Address is in sector 4 range */
        sector = FLASH_SECTOR_4;
    }
    else if (Address <= FLASH_END_ADDRESS)
    {
        /* Address is in sector 5 range */
        sector = FLASH_SECTOR_5;
    }
    else
    {
        /* Address is outside Flash range */
        sector = FLASH_SECTOR_TOTAL;
    }
    
    return sector;
}


/**
 * @brief  Program a buffer of data to Flash memory
 * @param  Address: Starting Flash address (alignment depends on ProgramSize)
 * @param  pData: Pointer to source data buffer
 * @param  Length: Number of bytes to write
 * @param  ProgramSize: Size of each write operation (byte/half-word/word/double-word)
 * @retval FLASH_Status_t: FLASH_OK or error code
 * @note   - Length must be compatible with ProgramSize:
 *           - Byte: any length
 *           - Half-word: length must be multiple of 2
 *           - Word: length must be multiple of 4
 *           - Double-word: length must be multiple of 8
 *         - Address alignment must match ProgramSize
 *         - Flash must be unlocked and erased before programming
 */
FLASH_Status_t FLASH_ProgramBuffer(uint32_t Address, const uint8_t *pData, 
                                    uint32_t Length, FLASH_ProgramSize_t ProgramSize)
{
    FLASH_Status_t status = FLASH_OK;
    uint32_t current_address = Address;
    uint32_t bytes_written = 0;
    uint32_t step_size = 0;  /* Bytes per write operation */
    
    /* Input validation: Check for null pointer */
    if (pData == NULL)
    {
        return FLASH_ERROR_NULL_PTR;
    }
    
    /* Input validation: Check length is non-zero */
    if (Length == 0)
    {
        return FLASH_ERROR_INVALID_SIZE;
    }
    
    /* Determine step size and validate length/alignment based on ProgramSize */
    switch (ProgramSize)
    {
        case FLASH_PROGRAM_BYTE:
            step_size = 1;
            /* No alignment restriction for byte programming */
            /* Length can be any value */
            break;
            
        case FLASH_PROGRAM_HALF_WORD:
            step_size = 2;
            /* Check address is half-word aligned */
            if ((Address & 0x1) != 0)
            {
                return FLASH_ERROR_PG_ALIGNMENT;
            }
            /* Check length is multiple of 2 */
            if ((Length & 0x1) != 0)
            {
                return FLASH_ERROR_INVALID_SIZE;
            }
            break;
            
        case FLASH_PROGRAM_WORD:
            step_size = 4;
            /* Check address is word aligned */
            if ((Address & 0x3) != 0)
            {
                return FLASH_ERROR_PG_ALIGNMENT;
            }
            /* Check length is multiple of 4 */
            if ((Length & 0x3) != 0)
            {
                return FLASH_ERROR_INVALID_SIZE;
            }
            break;
            
        case FLASH_PROGRAM_DOUBLE_WORD:
            step_size = 8;
            /* Check address is double-word aligned */
            if ((Address & 0x7) != 0)
            {
                return FLASH_ERROR_PG_ALIGNMENT;
            }
            /* Check length is multiple of 8 */
            if ((Length & 0x7) != 0)
            {
                return FLASH_ERROR_INVALID_SIZE;
            }
            break;
            
        default:
            return FLASH_ERROR_INVALID_SIZE;
    }
    
    /* Check that entire write range is within Flash */
    if (Address < FLASH_START_ADDRESS || 
        (Address + Length - 1) > FLASH_END_ADDRESS)
    {
        return FLASH_ERROR_INVALID_ADDRESS;
    }
    
    /* Program the buffer data to Flash */
    while (bytes_written < Length)
    {
        /* Call appropriate programming function based on ProgramSize */
        switch (ProgramSize)
        {
            case FLASH_PROGRAM_BYTE:
            {
                uint8_t data = pData[bytes_written];
                status = FLASH_ProgramByte(current_address, data);
                break;
            }
            
            case FLASH_PROGRAM_HALF_WORD:
            {
                /* Extract 16-bit value from buffer (little-endian) */
                uint16_t data = (uint16_t)pData[bytes_written] | 
                                ((uint16_t)pData[bytes_written + 1] << 8);
                status = FLASH_ProgramHalfWord(current_address, data);
                break;
            }
            
            case FLASH_PROGRAM_WORD:
            {
                /* Extract 32-bit value from buffer (little-endian) */
                uint32_t data = (uint32_t)pData[bytes_written] | 
                                ((uint32_t)pData[bytes_written + 1] << 8) |
                                ((uint32_t)pData[bytes_written + 2] << 16) |
                                ((uint32_t)pData[bytes_written + 3] << 24);
                status = FLASH_ProgramWord(current_address, data);
                break;
            }
            
            case FLASH_PROGRAM_DOUBLE_WORD:
            {
                /* Extract 64-bit value from buffer (little-endian) */
                uint64_t data = (uint64_t)pData[bytes_written] | 
                                ((uint64_t)pData[bytes_written + 1] << 8) |
                                ((uint64_t)pData[bytes_written + 2] << 16) |
                                ((uint64_t)pData[bytes_written + 3] << 24) |
                                ((uint64_t)pData[bytes_written + 4] << 32) |
                                ((uint64_t)pData[bytes_written + 5] << 40) |
                                ((uint64_t)pData[bytes_written + 6] << 48) |
                                ((uint64_t)pData[bytes_written + 7] << 56);
                status = FLASH_ProgramDoubleWord(current_address, data);
                break;
            }
            
            default:
                return FLASH_ERROR_INVALID_SIZE;
        }
        
        /* Check if programming operation succeeded */
        if (status != FLASH_OK)
        {
            /* Return error immediately - stop programming on first error */
            return status;
        }
        
        /* Move to next location */
        current_address += step_size;
        bytes_written += step_size;
    }
    
    return FLASH_OK;
}