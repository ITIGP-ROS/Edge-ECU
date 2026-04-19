#ifndef CRC_H
#define CRC_H

#include "../../../lib/STD_Types.h"

#define CRC32_POLYNOMIAL 0x04C11DB7UL

// Create doxygen documentation for each function

/**
 * @brief Initializes the CRC peripheral.
 * @return STD_ReturnType Returns STD_SUCCESS if initialization is successful, otherwise an error code.
 */
STD_ReturnType CRC_Init(void);

/**
 * @brief Deinitializes the CRC peripheral.
 * @return STD_ReturnType Returns STD_SUCCESS if deinitialization is successful, otherwise an error code.
 */
STD_ReturnType CRC_DeInit(void);

/**
 * @brief Accumulates CRC value over a buffer.  (Does not reset the CRC peripheral)
 * @param buffer Pointer to the data buffer.
 * @param length Length of the data buffer in 32-bit words.
 * @param crcResult Pointer to store the resulting CRC value.
 * @return STD_ReturnType Returns STD_SUCCESS if accumulation is successful, otherwise an error code.
 */
STD_ReturnType CRC_Accumulate(const uint32_t* buffer, uint32_t length, uint32_t* crcResult);

/**
 * @brief Calculates CRC value for a buffer. (Resets CRC before calculation)
 * @param buffer Pointer to the data buffer.
 * @param length Length of the data buffer in 32-bit words.
 * @param crcResult Pointer to store the resulting CRC value.
 * @return STD_ReturnType Returns STD_SUCCESS if calculation is successful, otherwise an error code.
 */
STD_ReturnType CRC_Calculate(const uint32_t* buffer, uint32_t length, uint32_t* crcResult);

/**
 * @brief Resets the CRC peripheral.
 * @return STD_ReturnType Returns STD_SUCCESS if reset is successful, otherwise an error code.
 */
STD_ReturnType CRC_Reset(void);

#endif // CRC_H