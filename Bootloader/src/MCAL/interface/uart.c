#include "interface/MCAL/uart.h"
#include "interface/MCAL/rcc.h"
#include "interface/MCAL/gpio.h"

void USART1_ITHandler(void);
void USART2_ITHandler(void);
void USART6_ITHandler(void);

static inline int8_t uart_index(const UART_Instance_t* uart)
{
    if(uart == UART1) return 0;
    if(uart == UART2) return 1;
    if(uart == UART6) return 2;
    return -1;
}

STD_ReturnType UART_Init(const UART_Config_t* uartObj, SYSTICK_ClockSource_t clockSource){
    STD_ReturnType ret = STD_SUCCESS;

    if(uartObj == NULL){
        ret = STD_ERROR;
    }
    else{
        // Get UART Instance Number
        int8_t uartNum = uart_index(uartObj->UartInstance);
        uint8_t altFunc = 0;
        if(uartNum < 0){
            ret = STD_ERROR;
            return ret;
        }
        // 0. Extract Clock Source Frequency
        uint32_t clockFreq = 0;
        switch(clockSource){
            case SYSTICK_CLOCK_SOURCE_HSI:
                clockFreq = 16000000; // 16 MHz
                break;
            case SYSTICK_CLOCK_SOURCE_HSE:
                clockFreq = 25000000;  // 25 MHz
                break;
            case SYSTICK_CLOCK_SOURCE_PLL_MAX:
                clockFreq = 84000000;  // 84 MHz
                break;
            default:
                ret = STD_ERROR;
                break;
        }
        if(uartNum == 1){
            clockFreq /= 2; // Divide Clock Frequenct by 2 for USART2 because its connected to APB1 (42MHz)
        }
        else{
            // Nothing
        }

        if(ret != STD_SUCCESS){
            return ret;
        }

        // 1. RCC Enable
        if(uartNum == 0){
            ret = RCC_ControlPeripheral(RCC_USART1, RCC_PERIPHERAL_ENABLE);
            altFunc = GPIO_AF7_USART1_2;
        }
        else if(uartNum == 1){
            ret = RCC_ControlPeripheral(RCC_USART2, RCC_PERIPHERAL_ENABLE);
            altFunc = GPIO_AF7_USART1_2;
        }
        else if(uartNum == 2){
            ret = RCC_ControlPeripheral(RCC_USART6, RCC_PERIPHERAL_ENABLE);
            altFunc = GPIO_AF8_USART6;
        }
        else{
            ret = STD_ERROR;
        }

        // 2. Baud Rate Selection
        uint32_t baudRate = uartObj->BaudRate;
        uint16_t usartdiv = 0;
        uint8_t fraction = 0;

        usartdiv =(clockFreq) /(16 * baudRate);   // oversampling by 16(OVER8 = 0)
        fraction =((clockFreq %(16 * baudRate)) * 16 +(16 * baudRate)/2) /(16 * baudRate);  // get rem*16 "to be from 0:15" then rounding Rounding
        usartdiv =(usartdiv << 4) | fraction;
        uartObj->UartInstance->BRR = usartdiv;

        // 3. Configure Data Bits
        if(uartObj->DataBits == UART_DATABITS_8){
            uartObj->UartInstance->CR1 &= ~(1 << 12);
        }
        else{
            uartObj->UartInstance->CR1 |=(1 << 12);
        }

        // 4. Configure Parity
        if(uartObj->Parity == UART_PARITY_NONE){
            uartObj->UartInstance->CR1 &= ~(1 << 10);
        }
        else if(uartObj->Parity == UART_PARITY_EVEN){
            uartObj->UartInstance->CR1 |=(1 << 10);
            uartObj->UartInstance->CR1 &= ~(1 << 9);
        }
        else{
            uartObj->UartInstance->CR1 |=(1 << 10);
            uartObj->UartInstance->CR1 |=(1 << 9);
        }

        // 5. Configure Stop Bits
        uartObj->UartInstance->CR2 &= ~(0b11 << 12); // 1 Stop Bit

        // 6. Enable Transmitter and Receiver
        uartObj->UartInstance->CR1 |=(1 << 3);  // Transmitter Enable
        uartObj->UartInstance->CR1 |=(1 << 2);  // Receiver Enable

        // 7. Enable UART
        uartObj->UartInstance->CR1 |=(1 << 13); // USART Enable

        // 8. Configure UART Pins
        GPIO_t uart_tx_pin = {
            .port       = uartObj->port,
            .pin        = uartObj->txPin,
            .mode       = GPIO_MODE_AF,
            .outputType = GPIO_OUTPUT_PUSHPULL,
            .speed      = GPIO_SPEED_HIGH,
            .pullType   = GPIO_NOPULL,
            .altFunc    = altFunc
        };
        ret = GPIO_Init(&uart_tx_pin);
        if(ret == STD_SUCCESS){
            GPIO_t uart_rx_pin = {
                .port       = uartObj->port,
                .pin        = uartObj->txPin + 1,
                .mode       = GPIO_MODE_AF,
                .outputType = GPIO_OUTPUT_PUSHPULL,
                .speed      = GPIO_SPEED_HIGH,
                .pullType   = GPIO_PULLUP,
                .altFunc    = altFunc
            };
            ret = GPIO_Init(&uart_rx_pin);
        }
        else{
            ret = STD_ERROR;
        }
    }
    return ret;
}

STD_ReturnType UART_DeInit(const UART_Config_t* uartObj){
    STD_ReturnType ret = STD_SUCCESS;
    
    if(uartObj == NULL){
        ret = STD_ERROR;
    }
    else{
        uartObj->UartInstance->CR1 = 0U;  // Reset Control Registers 1
        uartObj->UartInstance->CR2 = 0U;  // Reset Control Registers 2
        uartObj->UartInstance->CR3 = 0U;  // Reset Control Registers 3
        uartObj->UartInstance->BRR = 0U;  // Reset Baud Rate Register

        // RCC Disable
        int8_t uartNum = uart_index(uartObj->UartInstance);
        if(uartNum == 0){
            ret = RCC_ControlPeripheral(RCC_USART1, RCC_PERIPHERAL_DISABLE);
        }
        else if(uartNum == 1){
            ret = RCC_ControlPeripheral(RCC_USART2, RCC_PERIPHERAL_DISABLE);
        }
        else if(uartNum == 2){
            ret = RCC_ControlPeripheral(RCC_USART6, RCC_PERIPHERAL_DISABLE);
        }
        else{
            ret = STD_ERROR;
        }
    }
    return ret;
}

STD_ReturnType UART_SendChar(const UART_Config_t* uartObj, uint8_t data, uint32_t timeoutMS){
    STD_ReturnType ret = STD_SUCCESS;
    uint32_t currentTime = 0;

    if(uartObj == NULL){
        ret = STD_ERROR;
    }
    else{
        // Wait until Transmit Data Register Empty
        while((!(uartObj->UartInstance->SR &(1 << 7))) && currentTime < timeoutMS){
            currentTime++;
        }
        if(currentTime >= timeoutMS){
            ret = STD_TIMEOUT;
            return ret;
        }
        // Send Data
        uartObj->UartInstance->DR = data;
        // Wait until Transmission Complete
        currentTime = 0;
        while(!(uartObj->UartInstance->SR &(1 << 6)) && currentTime < timeoutMS){
            currentTime++;
        }
        if(currentTime >= timeoutMS){
            return STD_TIMEOUT;
        }

    }
    return ret;
}

STD_ReturnType UART_SendBuffer(const UART_Config_t* uartObj, Buffer_t* buffer, uint32_t timeoutMS){
    STD_ReturnType ret = STD_SUCCESS;
    
    if(uartObj == NULL || buffer == NULL){
        ret = STD_ERROR;
    }
    else{
        for(uint8_t idx = 0; idx < buffer->length; idx++){  // index, don't mutate pointer
            ret = UART_SendChar(uartObj, buffer->data[idx], timeoutMS);
            if(ret != STD_SUCCESS){
                break;
            }
        }
    }
    return ret;
}

STD_ReturnType UART_ReceiveChar(const UART_Config_t* uartObj, uint8_t* data, uint32_t timeoutMS){
    STD_ReturnType ret = STD_SUCCESS;
    uint32_t currentTime = 0;
    
    if(uartObj == NULL || data == NULL){
        ret = STD_ERROR;
    }
    else{
        // Wait until Read Data Register Not Empty
        while((!(uartObj->UartInstance->SR &(1 << 5))) && currentTime < timeoutMS){
            currentTime++;
        }
        if(currentTime >= timeoutMS){
            return STD_TIMEOUT;
        }
        // Read Data
        *data =(uint8_t)(uartObj->UartInstance->DR & 0xFF);
    }
    return ret;
}

STD_ReturnType UART_ReceiveBuffer(const UART_Config_t* uartObj, Buffer_t* buffer, uint32_t timeoutMS){
    STD_ReturnType ret = STD_SUCCESS;
    
    if(uartObj == NULL || buffer == NULL){
        ret = STD_ERROR;
    }
    else{
        for(uint8_t idx = 0; idx < buffer->length; idx++){
            ret = UART_ReceiveChar(uartObj, &buffer->data[idx], timeoutMS);
            if(ret != STD_SUCCESS){ 
                break; 
            }
        }
    }
    return ret;
}

