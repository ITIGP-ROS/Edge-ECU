#ifndef UART_DEBUG_H
#define UART_DEBUG_H

#include "STD_TYPES.h"

void UART_Debug_Init(void);
void UART_Debug_SendStr(const char *str);
void UART_Debug_SendUInt(uint32_t val);

#endif