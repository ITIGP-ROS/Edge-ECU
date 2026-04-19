#ifndef H_SERIAL_H
#define H_SERIAL_H

#include "../../../lib/STD_Types.h"
#include "interface/MCAL/uart.h"
#include "interface/MCAL/dma.h"

typedef struct{
    union{
        struct{
            uint8_t* data;
            uint8_t length;
            uint8_t index;
        } buffer;
        struct{
            uint8_t* src;
            uint8_t* dest;
            uint32_t length;
        } dmaBuffer;
    };
} HSerial_Buffer_t;

typedef struct {
    UART_Config_t* uartConfig;
    DMA_Instance_t* txDma;
    DMA_Instance_t* rxDma;
    HSerial_Buffer_t* txBuffer;
    HSerial_Buffer_t* rxBuffer;
} HSerial_Config_t;

/**
 * @brief Initialize the high-level serial driver.
 *
 * @param hserialConfig Pointer to serial configuration structure.
 * @param clockSource   Systick clock source used for timing.
 * @return STD_SUCCESS on success, otherwise STD_ERROR.
 */
STD_ReturnType HSerial_Init(HSerial_Config_t* hserialConfig, SYSTICK_ClockSource_t clockSource);

/**
 * @brief De-initialize the high-level serial driver.
 *
 * @param hserialConfig Pointer to serial configuration structure.
 * @return STD_SUCCESS on success, otherwise STD_ERROR.
 */
STD_ReturnType HSerial_DeInit(HSerial_Config_t* hserialConfig);

/**
 * @brief Send a buffer in blocking mode.
 *
 * @param hserialConfig Pointer to serial configuration structure.
 * @param timeoutMS     Timeout in milliseconds.
 * @return STD_SUCCESS on success, otherwise STD_TIMEOUT or STD_ERROR.
 */
STD_ReturnType HSerial_SendBuffer(HSerial_Config_t* hserialConfig, uint32_t timeoutMS);

/**
 * @brief Receive a buffer in blocking mode.
 *
 * @param hserialConfig Pointer to serial configuration structure.
 * @param timeoutMS     Timeout in milliseconds.
 * @return STD_SUCCESS on success, otherwise STD_TIMEOUT or STD_ERROR.
 */
STD_ReturnType HSerial_ReceiveBuffer(HSerial_Config_t* hserialConfig, uint32_t timeoutMS);

/**
 * @brief Send a buffer using interrupt mode.
 *
 * @param hserialConfig Pointer to serial configuration structure.
 * @return STD_SUCCESS on success, otherwise STD_ERROR.
 */
STD_ReturnType HSerial_SendBufferIT(HSerial_Config_t* hserialConfig);

/**
 * @brief Receive a buffer using interrupt mode.
 *
 * @param hserialConfig Pointer to serial configuration structure.
 * @return STD_SUCCESS on success, otherwise STD_ERROR.
 */
STD_ReturnType HSerial_ReceiveBufferIT(HSerial_Config_t* hserialConfig);

/**
 * @brief Send a buffer using DMA mode.
 *
 * @param hserialConfig Pointer to serial configuration structure.
 * @return STD_SUCCESS on success, otherwise STD_ERROR.
 */
STD_ReturnType HSerial_SendBufferDMA(HSerial_Config_t* hserialConfig);

/**
 * @brief Receive a buffer using DMA mode.
 *
 * @param hserialConfig Pointer to serial configuration structure.
 * @return STD_SUCCESS on success, otherwise STD_ERROR.
 */
STD_ReturnType HSerial_ReceiveBufferDMA(HSerial_Config_t* hserialConfig);

/**
 * @brief Get current DMA transmit state.
 *
 * @param hserialConfig Pointer to serial configuration structure.
 * @param state         Output: current DMA TX state.
 * @return STD_SUCCESS on success, otherwise STD_ERROR.
 */
STD_ReturnType HSerial_TxGetStateDMA(HSerial_Config_t* hserialConfig, DMA_State_t* state);

/**
 * @brief Get current DMA receive state.
 *
 * @param hserialConfig Pointer to serial configuration structure.
 * @param state         Output: current DMA RX state.
 * @return STD_SUCCESS on success, otherwise STD_ERROR.
 */
STD_ReturnType HSerial_RxGetStateDMA(HSerial_Config_t* hserialConfig, DMA_State_t* state);

#endif // H_SERIAL_H