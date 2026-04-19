#include "private/MCAL/crc_priv.h"
#include "interface/MCAL/crc.h"
#include "interface/MCAL/rcc.h"

STD_ReturnType CRC_Init(void){
    // Enable CRC clock
    STD_ReturnType status = RCC_ControlPeripheral(RCC_CRC, RCC_PERIPHERAL_ENABLE);

    return status;
}

STD_ReturnType CRC_DeInit(void){
    // Disable CRC clock
    STD_ReturnType ret = RCC_ControlPeripheral(RCC_CRC, RCC_PERIPHERAL_DISABLE);

    return ret;
}


STD_ReturnType CRC_Accumulate(const uint32_t* buffer, uint32_t length, uint32_t* crcResult){
    uint32_t index;

    // Enter Data to the CRC calculator
    for (index = 0U; index < length; index++){
        CRC->DR = buffer[index];
    }
    *crcResult = CRC->DR;

    return STD_SUCCESS;
}

STD_ReturnType CRC_Calculate(const uint32_t* buffer, uint32_t length, uint32_t* crcResult){
    STD_ReturnType ret;
    ret = CRC_Reset();
    ret = CRC_Accumulate(buffer, length, crcResult);
    return ret;
}

STD_ReturnType CRC_Reset(void){
    CRC->CR |= 1U;
    return STD_SUCCESS;
}