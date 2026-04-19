#include "interface/HAL/hserial.h"

volatile uint8_t txBuffer[3][100] = {0};
volatile uint8_t* requestedTxBuffer[3] = {NULL};
volatile uint32_t requestedTxLength[3] = {0};
volatile uint8_t  requestedTxIndex[3] = {0};
volatile uint8_t* requestedRxBuffer[3] = {NULL};
volatile uint32_t requestedRxLength[3] = {0};
volatile uint8_t  requestedRxIndex[3] = {0};

CBFunc_t txCallback[3] ={NULL};
CBFunc_t rxCallback[3] ={NULL};

void USART_Handler(uint8_t uartNum, UART_Instance_t uartInstance){
    // TXE handling
    if(UART_GetTXEFlag(uartInstance) && UART_GetTXIE(uartInstance)){
        if(requestedTxLength[uartNum] > 0 && requestedTxBuffer[uartNum] != NULL){
            UART_SetDR(uartInstance, *(requestedTxBuffer[uartNum]++));
            requestedTxLength[uartNum]--;
            requestedTxIndex[uartNum]++;
        }
        if(requestedTxLength[uartNum] == 0){
            UART_SetTXIE(uartInstance, UART_INTERRUPT_DISABLE);
            requestedTxBuffer[uartNum] = NULL;
            if(txCallback[uartNum] != NULL){
                txCallback[uartNum]();
            }
        }
    }
    // RXNE handling
    if(UART_GetRXNEFlag(uartInstance) && UART_GetRXIE(uartInstance)){
        uint8_t data = UART_GetDR(uartInstance);
        if(requestedRxLength[uartNum] > 0){
            *(requestedRxBuffer[uartNum]++) = data;
            requestedRxLength[uartNum]--;
            requestedRxIndex[uartNum]++;
        }
        if(requestedRxLength[uartNum] == 0){
            UART_SetRXIE(uartInstance, UART_INTERRUPT_DISABLE);
            requestedRxBuffer[uartNum] = NULL;
            if(rxCallback[uartNum] != NULL){
                rxCallback[uartNum]();
            }
        }
    }
}

void USART1_ITHandler(void){
    USART_Handler(0, UART1);
}
void USART2_ITHandler(void){
    USART_Handler(1, UART2);
}
void USART6_ITHandler(void){
    USART_Handler(2, UART6);
}

static inline int8_t uart_index(const UART_Instance_t* uart){
    if(uart == UART1) return 0;
    if(uart == UART2) return 1;
    if(uart == UART6) return 2;
    return -1;
}

STD_ReturnType HSerial_Init(HSerial_Config_t* hserialConfig, SYSTICK_ClockSource_t clockSource){
    STD_ReturnType ret = STD_SUCCESS;

    if(hserialConfig == NULL){
        ret = STD_ERROR;
    }
    else{
        ret = UART_Init(hserialConfig->uartConfig, clockSource);
        int8_t uartNum = uart_index(hserialConfig->uartConfig->UartInstance);
        if(uartNum < 0){
            ret = STD_ERROR;
            return ret;
        }
        // Set Callbacks
        txCallback[uartNum] = hserialConfig->uartConfig->txCallback;
        rxCallback[uartNum] = hserialConfig->uartConfig->rxCallback;
        if(ret != STD_SUCCESS){
            return ret;
        }

        if(hserialConfig->uartConfig->dmaEnable == UART_DMA_ENABLE){
            ret = DMA_Init(hserialConfig->txDma);
            if(ret != STD_SUCCESS){
                return ret;
            }

            ret = DMA_Init(hserialConfig->rxDma);
            if(ret != STD_SUCCESS){
                return ret;
            }
        }
        else{
            // Nothing
        }
    }

    return ret;
}

STD_ReturnType HSerial_DeInit(HSerial_Config_t* hserialConfig){
    STD_ReturnType ret = STD_SUCCESS;

    if(hserialConfig == NULL){
        ret = STD_ERROR;
    }
    else{
        if(hserialConfig->uartConfig->dmaEnable == UART_DMA_ENABLE){
            ret = DMA_DeInit(hserialConfig->txDma);
            if(ret != STD_SUCCESS){
                return ret;
            }

            ret = DMA_DeInit(hserialConfig->rxDma);
            if(ret != STD_SUCCESS){
                return ret;
            }
        }
        else{
            // Nothing
        }

        ret = UART_DeInit(hserialConfig->uartConfig);
        if(ret != STD_SUCCESS){
            return ret;
        }
    }

    return ret;
}

STD_ReturnType HSerial_SendBuffer(HSerial_Config_t* hserialConfig, uint32_t timeoutMS){
    STD_ReturnType ret = STD_SUCCESS;

    if(hserialConfig == NULL){
        ret = STD_ERROR;
    }
    else{
        ret = UART_SendBuffer(hserialConfig->uartConfig, (Buffer_t*)hserialConfig->txBuffer, timeoutMS);
    }

    return ret;
}

STD_ReturnType HSerial_SendBufferIT(HSerial_Config_t* hserialConfig){
    STD_ReturnType ret = STD_SUCCESS;

    if(hserialConfig == NULL){
        ret = STD_ERROR;
    }
    else{
        int8_t uartNum = uart_index(hserialConfig->uartConfig->UartInstance);
        if(uartNum < 0){
            return STD_ERROR;
        }

        if(requestedTxBuffer[uartNum] != NULL){
            return STD_BUSY;
        }

        if(hserialConfig->txBuffer->buffer.length > sizeof(txBuffer[uartNum])){
            return STD_ERROR;
        }

        for(uint8_t i = 0; i < hserialConfig->txBuffer->buffer.length; i++){
            txBuffer[uartNum][i] = hserialConfig->txBuffer->buffer.data[i];
        }
        requestedTxBuffer[uartNum] = &txBuffer[uartNum][0];
        requestedTxLength[uartNum] = hserialConfig->txBuffer->buffer.length;
        hserialConfig->txBuffer->buffer.index = requestedTxIndex[uartNum] = 0;

        UART_SetTXIE(hserialConfig->uartConfig->UartInstance, UART_INTERRUPT_ENABLE);
    }

    return ret;
}

STD_ReturnType HSerial_SendBufferDMA(HSerial_Config_t* hserialConfig){
    STD_ReturnType ret = STD_SUCCESS;

    if(hserialConfig == NULL){
        ret = STD_ERROR;
    }
    else{        
        DMA_State_t state = 0;
        ret = DMA_GetState(hserialConfig->txDma, &state);
        if(state == DMA_STATE_READY){
            // Start DMA
            ret = DMA_Start(hserialConfig->txDma, (uint32_t)(hserialConfig->txBuffer->dmaBuffer.src), (uint32_t)&(hserialConfig->uartConfig->UartInstance->DR), hserialConfig->txBuffer->dmaBuffer.length);
        }
        else if(state == DMA_STATE_BUSY){
            ret = STD_BUSY;
        }
        else{
            ret = STD_ERROR;
        }
    }

    return ret;
}

STD_ReturnType HSerial_ReceiveBuffer(HSerial_Config_t* hserialConfig, uint32_t timeoutMS){
    STD_ReturnType ret = STD_SUCCESS;

    if(hserialConfig == NULL){
        ret = STD_ERROR;
    }
    else{
        ret = UART_ReceiveBuffer(hserialConfig->uartConfig, (Buffer_t*)hserialConfig->rxBuffer, timeoutMS);
    }

    return ret;
}

STD_ReturnType HSerial_ReceiveBufferIT(HSerial_Config_t* hserialConfig){
    STD_ReturnType ret = STD_SUCCESS;

    if(hserialConfig == NULL){
        ret = STD_ERROR;
    }
    else{
        int8_t uartNum = uart_index(hserialConfig->uartConfig->UartInstance);
        if(uartNum < 0){
            ret = STD_ERROR;
        }
        else{
            if(requestedRxBuffer[uartNum] != NULL){
                return STD_BUSY;
            }
            else{
                requestedRxBuffer[uartNum] = hserialConfig->rxBuffer->buffer.data;
                requestedRxLength[uartNum] = hserialConfig->rxBuffer->buffer.length;
                hserialConfig->rxBuffer->buffer.index = 0;
                requestedRxIndex[uartNum] = 0;

                UART_SetRXIE(hserialConfig->uartConfig->UartInstance, UART_INTERRUPT_ENABLE);
            }
        }
    }

    return ret;
}

STD_ReturnType HSerial_ReceiveBufferDMA(HSerial_Config_t* hserialConfig){
    STD_ReturnType ret = STD_SUCCESS;

    if(hserialConfig == NULL){
        ret = STD_ERROR;
    }
    else{        
        DMA_State_t state = 0;
        ret = DMA_GetState(hserialConfig->rxDma, &state);
        if(state == DMA_STATE_READY){
            // Start DMA
            ret = DMA_Start(hserialConfig->rxDma, (uint32_t)&(hserialConfig->uartConfig->UartInstance->DR), (uint32_t)(hserialConfig->rxBuffer->dmaBuffer.dest), hserialConfig->rxBuffer->dmaBuffer.length);
        }
        else if(state == DMA_STATE_BUSY){
            ret = STD_BUSY;
        }
        else{
            ret = STD_ERROR;
        }
    }

    return ret;
}

STD_ReturnType HSerial_TxGetStateDMA(HSerial_Config_t* hserialConfig, DMA_State_t* state){
    STD_ReturnType ret = STD_SUCCESS;
    if(hserialConfig == NULL){
        ret = STD_ERROR;
    }
    else{
        ret = DMA_GetState(hserialConfig->txDma, state);
    }
    return ret;
}

STD_ReturnType HSerial_RxGetStateDMA(HSerial_Config_t* hserialConfig, DMA_State_t* state){
    STD_ReturnType ret = STD_SUCCESS;
    if(hserialConfig == NULL){
        ret = STD_ERROR;
    }
    else{
        ret = DMA_GetState(hserialConfig->rxDma, state);
    }
    return ret;
}