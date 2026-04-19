#ifndef UART_PRIV_H
#define UART_PRIV_H

#include "../../../lib/STD_Types.h"

typedef struct{
    volatile uint32_t SR;     // 0x00
    volatile uint32_t DR;     // 0x04
    volatile uint32_t BRR;    // 0x08
    volatile uint32_t CR1;    // 0x0C
    volatile uint32_t CR2;    // 0x10
    volatile uint32_t CR3;    // 0x14
    volatile uint32_t GTPR;   // 0x18
} UART_Reg_t;

#define UART1   ((UART_Reg_t*)0x40011000UL)
#define UART2   ((UART_Reg_t*)0x40004400UL)
#define UART6   ((UART_Reg_t*)0x40011400UL)

typedef UART_Reg_t* UART_Instance_t;

#endif