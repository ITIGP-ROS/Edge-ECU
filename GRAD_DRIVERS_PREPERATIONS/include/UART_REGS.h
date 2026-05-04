#ifndef UART_REGS_H
#define UART_REGS_H

#include "STD_TYPES.h"

/* Base addresses (RM0368 / RM0090) */
#define UART1_BASE_ADDR   0x40011000UL   /* USART1 on APB2 */
#define UART2_BASE_ADDR   0x40004400UL   /* USART2 on APB1 */
#define UART6_BASE_ADDR   0x40011400UL   /* USART6 on APB2 */

/* =======================
 *   UART register layout
 * ======================= */
typedef struct
{
    /* 0x00: Status register (USART_SR) */
    union {
        volatile uint32_t ALL;        /* U-01: volatile kept on ALL — covers the whole word */
        struct {
            uint32_t PE    : 1;  /* Bit 0  Parity error       */  /* U-01: volatile removed from bit-field */
            uint32_t FE    : 1;  /* Bit 1  Framing error      */  /* U-01: volatile removed from bit-field */
            uint32_t NF    : 1;  /* Bit 2  Noise flag         */  /* U-01: volatile removed from bit-field */
            uint32_t ORE   : 1;  /* Bit 3  Overrun error      */  /* U-01: volatile removed from bit-field */
            uint32_t IDLE  : 1;  /* Bit 4  IDLE line detected */  /* U-01: volatile removed from bit-field */
            uint32_t RXNE  : 1;  /* Bit 5  RX not empty       */  /* U-01: volatile removed from bit-field */
            uint32_t TC    : 1;  /* Bit 6  Transmission compl */  /* U-01: volatile removed from bit-field */
            uint32_t TXE   : 1;  /* Bit 7  TX data empty      */  /* U-01: volatile removed from bit-field */
            uint32_t LBD   : 1;  /* Bit 8  LIN break detect   */  /* U-01: volatile removed from bit-field */
            uint32_t CTS   : 1;  /* Bit 9  CTS flag           */  /* U-01: volatile removed from bit-field */
            uint32_t RES   :22;  /* 31:10 Reserved            */  /* U-01: volatile removed from bit-field */
        } BITS;
    } SR;

    /* 0x04: Data register (USART_DR) */
    union {
        volatile uint32_t ALL;        /* U-01: volatile kept on ALL only */
        struct {
            uint32_t DR    : 9;  /* 8:0 data value            */  /* U-01: volatile removed from bit-field */
            uint32_t RES   :23;  /* 31:9 reserved             */  /* U-01: volatile removed from bit-field */
        } BITS;
    } DR;

    /* 0x08: Baud rate register (USART_BRR) */
    union {
        volatile uint32_t ALL;        /* U-01: volatile kept on ALL only */
        struct {
            uint32_t DIV_Fraction : 4;  /* 3:0  fraction */  /* U-01: volatile removed from bit-field */
            uint32_t DIV_Mantissa :12;  /*15:4  mantissa */  /* U-01: volatile removed from bit-field */
            uint32_t RES          :16;  /*31:16 reserved */  /* U-01: volatile removed from bit-field */
        } BITS;
    } BRR;

    /* 0x0C: Control register 1 (USART_CR1) */
    union {
        volatile uint32_t ALL;        /* U-01: volatile kept on ALL only */
        struct {
            uint32_t SBK    : 1;  /* Bit 0  Send break        */  /* U-01: volatile removed from bit-field */
            uint32_t RWU    : 1;  /* Bit 1  Receiver wakeup   */  /* U-01: volatile removed from bit-field */
            uint32_t RE     : 1;  /* Bit 2  Receiver enable   */  /* U-01: volatile removed from bit-field */
            uint32_t TE     : 1;  /* Bit 3  Transmitter en    */  /* U-01: volatile removed from bit-field */
            uint32_t IDLEIE : 1;  /* Bit 4  IDLE interrupt    */  /* U-01: volatile removed from bit-field */
            uint32_t RXNEIE : 1;  /* Bit 5  RXNE interrupt    */  /* U-01: volatile removed from bit-field */
            uint32_t TCIE   : 1;  /* Bit 6  TC interrupt      */  /* U-01: volatile removed from bit-field */
            uint32_t TXEIE  : 1;  /* Bit 7  TXE interrupt     */  /* U-01: volatile removed from bit-field */
            uint32_t PEIE   : 1;  /* Bit 8  PE interrupt      */  /* U-01: volatile removed from bit-field */
            uint32_t PS     : 1;  /* Bit 9  Parity selection  */  /* U-01: volatile removed from bit-field */
            uint32_t PCE    : 1;  /* Bit10 Parity control en  */  /* U-01: volatile removed from bit-field */
            uint32_t WAKE   : 1;  /* Bit11 Wakeup method      */  /* U-01: volatile removed from bit-field */
            uint32_t M      : 1;  /* Bit12 Word length        */  /* U-01: volatile removed from bit-field */
            uint32_t UE     : 1;  /* Bit13 USART enable       */  /* U-01: volatile removed from bit-field */
            uint32_t RES0   : 1;  /* Bit14 Reserved           */  /* U-01: volatile removed from bit-field */
            uint32_t OVER8  : 1;  /* Bit15 Oversampling by 8  */  /* U-01: volatile removed from bit-field */
            uint32_t RES1   :16;  /*31:16 Reserved            */  /* U-01: volatile removed from bit-field */
        } BITS;
    } CR1;

    /* 0x10: Control register 2 (USART_CR2) */
    union {
        volatile uint32_t ALL;        /* U-01: volatile kept on ALL only */
        struct {
            uint32_t ADD    : 4;  /* 3:0  Address of USART node */  /* U-01: volatile removed from bit-field */
            uint32_t RES0   : 1;  /* Bit 4 Reserved             */  /* U-01: volatile removed from bit-field */
            uint32_t LBDL   : 1;  /* Bit 5 LIN break length     */  /* U-01: volatile removed from bit-field */
            uint32_t LBDIE  : 1;  /* Bit 6 LIN break int en     */  /* U-01: volatile removed from bit-field */
            uint32_t RES1   : 1;  /* Bit 7 Reserved             */  /* U-01: volatile removed from bit-field */
            uint32_t LBCL   : 1;  /* Bit 8 Last bit clock pulse */  /* U-01: volatile removed from bit-field */
            uint32_t CPHA   : 1;  /* Bit 9 Clock phase          */  /* U-01: volatile removed from bit-field */
            uint32_t CPOL   : 1;  /* Bit10 Clock polarity       */  /* U-01: volatile removed from bit-field */
            uint32_t CLKEN  : 1;  /* Bit11 Clock enable         */  /* U-01: volatile removed from bit-field */
            uint32_t STOP   : 2;  /*13:12 Stop bits             */  /* U-01: volatile removed from bit-field */
            uint32_t LINEN  : 1;  /* Bit14 LIN mode enable      */  /* U-01: volatile removed from bit-field */
            uint32_t RES2   :17;  /*31:15 Reserved              */  /* U-01: volatile removed from bit-field */
        } BITS;
    } CR2;

    /* 0x14: Control register 3 (USART_CR3) */
    union {
        volatile uint32_t ALL;        /* U-01: volatile kept on ALL only */
        struct {
            uint32_t EIE    : 1;  /* Bit 0  Error int enable   */  /* U-01: volatile removed from bit-field */
            uint32_t IREN   : 1;  /* Bit 1  IrDA mode enable   */  /* U-01: volatile removed from bit-field */
            uint32_t IRLP   : 1;  /* Bit 2  IrDA low power     */  /* U-01: volatile removed from bit-field */
            uint32_t HDSEL  : 1;  /* Bit 3  Half-duplex sel    */  /* U-01: volatile removed from bit-field */
            uint32_t NACK   : 1;  /* Bit 4  Smartcard NACK     */  /* U-01: volatile removed from bit-field */
            uint32_t SCEN   : 1;  /* Bit 5  Smartcard mode en  */  /* U-01: volatile removed from bit-field */
            uint32_t DMAR   : 1;  /* Bit 6  DMA enable RX      */  /* U-01: volatile removed from bit-field */
            uint32_t DMAT   : 1;  /* Bit 7  DMA enable TX      */  /* U-01: volatile removed from bit-field */
            uint32_t RTSE   : 1;  /* Bit 8  RTS enable         */  /* U-01: volatile removed from bit-field */
            uint32_t CTSE   : 1;  /* Bit 9  CTS enable         */  /* U-01: volatile removed from bit-field */
            uint32_t CTSIE  : 1;  /* Bit10 CTS interrupt en    */  /* U-01: volatile removed from bit-field */
            uint32_t ONEBIT : 1;  /* Bit11 One sample bit      */  /* U-01: volatile removed from bit-field */
            uint32_t RES    :20;  /*31:12 Reserved             */  /* U-01: volatile removed from bit-field */
        } BITS;
    } CR3;

    /* 0x18: Guard time and prescaler register (USART_GTPR) */
    union {
        volatile uint32_t ALL;        /* U-01: volatile kept on ALL only */
        struct {
            uint32_t PSC    : 8;  /* 7:0  Prescaler value      */  /* U-01: volatile removed from bit-field */
            uint32_t GT     : 8;  /*15:8 Guard time            */  /* U-01: volatile removed from bit-field */
            uint32_t RES    :16;  /*31:16 Reserved             */  /* U-01: volatile removed from bit-field */
        } BITS;
    } GTPR;

} UART_REGS_t;

#define USART_SR_TXE        0x80U
#define USART_SR_TC         0x40U
#define USART_SR_RXNE       0x20U
#define USART_SR_IDLE       0x10U
#define USART_SR_ORE        0x08U
#define USART_SR_NF         0x04U
#define USART_SR_FE         0x02U
#define USART_SR_PE         0x01U
#define USART_SR_LBD        (1U << 8U)

/* Handy pointers — volatile required for correct MMIO access */
#define UART1   ((volatile UART_REGS_t *)UART1_BASE_ADDR)  /* U-02: added volatile — prevents optimiser caching MMIO reads */
#define UART2   ((volatile UART_REGS_t *)UART2_BASE_ADDR)  /* U-02: added volatile — prevents optimiser caching MMIO reads */
#define UART6   ((volatile UART_REGS_t *)UART6_BASE_ADDR)  /* U-02: added volatile — prevents optimiser caching MMIO reads */

#endif /* UART_REGS_H */
