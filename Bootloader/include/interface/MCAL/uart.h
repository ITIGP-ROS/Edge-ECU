#ifndef UART_H
#define UART_H

#include "private/MCAL/uart_priv.h"
#include "interface/Core/systick.h"

typedef enum{   
    UART_BAUDRATE_9600   = 9600,
    UART_BAUDRATE_19200  = 19200,
    UART_BAUDRATE_38400  = 38400,
    UART_BAUDRATE_57600  = 57600,
    UART_BAUDRATE_115200 = 115200,
    UART_BAUDRATE_230400 = 230400,
    UART_BAUDRATE_460800 = 460800,
    UART_BAUDRATE_921600 = 921600,
} UART_BaudRate_t;

typedef enum{
    UART_PARITY_NONE = 0,
    UART_PARITY_EVEN = 1,
    UART_PARITY_ODD  = 2
} UART_Parity_t;

typedef enum{
    UART_DATABITS_8  = 8,
    UART_DATABITS_9  = 9
} UART_DataBits_t;

typedef void (*CBFunc_t)(void);

#define UART_INTERRUPT_ENABLE    1
#define UART_INTERRUPT_DISABLE   0

#define UART_DMA_ENABLE    1
#define UART_DMA_DISABLE   0

typedef struct{
    uint8_t* data;
    uint8_t length;
    uint8_t index;
} Buffer_t;

typedef struct{
    UART_Instance_t UartInstance;
    UART_BaudRate_t BaudRate;
    UART_DataBits_t DataBits;
    UART_Parity_t Parity;
    uint8_t port;
    uint8_t txPin;
    CBFunc_t txCallback;
    CBFunc_t rxCallback;
    uint8_t dmaEnable;
} UART_Config_t;

STD_ReturnType UART_Init(const UART_Config_t* uartObj, SYSTICK_ClockSource_t clockSource);
STD_ReturnType UART_DeInit(const UART_Config_t* uartObj);
STD_ReturnType UART_SendChar(const UART_Config_t* uartObj, uint8_t data, uint32_t timeoutMS);
STD_ReturnType UART_SendBuffer(const UART_Config_t* uartObj, Buffer_t* buffer, uint32_t timeoutMS);
STD_ReturnType UART_ReceiveChar(const UART_Config_t* uartObj, uint8_t* data, uint32_t timeoutMS);
STD_ReturnType UART_ReceiveBuffer(const UART_Config_t* uartObj, Buffer_t* buffer, uint32_t timeoutMS);

STD_ReturnType UART_SendCharIT(const UART_Config_t* uartObj, uint8_t data);
STD_ReturnType UART_SendBufferIT(const UART_Config_t* uartObj, Buffer_t* buffer);
STD_ReturnType UART_ReceiveCharIT(const UART_Config_t* uartObj, uint8_t* data);
STD_ReturnType UART_ReceiveBufferIT(const UART_Config_t* uartObj, Buffer_t* buffer);

// Getters & Setters
void UART_SetTXIE(UART_Instance_t uartInstance, uint8_t status);
void UART_SetRXIE(UART_Instance_t uartInstance, uint8_t status);

uint8_t UART_GetTXIE(UART_Instance_t uartInstance);
uint8_t UART_GetRXIE(UART_Instance_t uartInstance);

uint8_t UART_GetTXEFlag(UART_Instance_t uartInstance);
uint8_t UART_GetRXNEFlag(UART_Instance_t uartInstance);

void UART_SetDR(UART_Instance_t uartInstance, uint8_t data);
uint8_t UART_GetDR(UART_Instance_t uartInstance);

#endif // UART_H