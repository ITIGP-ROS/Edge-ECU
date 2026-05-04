#ifndef I2C_REGS_H
#define I2C_REGS_H

/**
 * @file    I2C_REGS.h
 * @brief   Complete I2C Register Definitions for STM32F401xx
 * @author  Based on RM0368 Reference Manual
 *
 * This header provides bit-field access to all I2C peripheral registers
 * using union structures for both direct (ALL) and bit-level (BITS) access.
 *
 * Fixes applied:
 *   I2C-R01  Removed volatile from every bit-field member; kept only on ALL.
 *   I2C-R02  Added UL suffix to all three base-address macros (MISRA 7.2).
 *   I2C-R03  Cast peripheral pointer macros to (volatile I2C_REGS_t *).
 *   I2C-R05  Annotated I2C_SPEED_FMP_HZ as unsupported on STM32F401CC.
 */

#include "STD_TYPES.h"


/**
 * ============================================================================
 * I2C_CR1 - Control Register 1
 * Address offset: 0x00
 * Reset value: 0x0000
 * ============================================================================
 */
typedef union {
    volatile uint32_t ALL;
    struct {
        uint32_t PE         : 1;  /* Bit 0:  Peripheral Enable                    */
        uint32_t SMBUS      : 1;  /* Bit 1:  SMBus Mode                           */
        uint32_t RESERVED0  : 1;  /* Bit 2:  Reserved                             */
        uint32_t SMBTYPE    : 1;  /* Bit 3:  SMBus Type                           */
        uint32_t ENARP      : 1;  /* Bit 4:  ARP Enable (SMBus)                   */
        uint32_t ENPEC      : 1;  /* Bit 5:  PEC Enable                           */
        uint32_t ENGC       : 1;  /* Bit 6:  General Call Enable                  */
        uint32_t NOSTRETCH  : 1;  /* Bit 7:  Clock Stretching Disable (Slave)     */
        uint32_t START      : 1;  /* Bit 8:  Start Generation                     */
        uint32_t STOP       : 1;  /* Bit 9:  Stop Generation                      */
        uint32_t ACK        : 1;  /* Bit 10: Acknowledge Enable                   */
        uint32_t POS        : 1;  /* Bit 11: Acknowledge/PEC Position             */
        uint32_t PEC        : 1;  /* Bit 12: Packet Error Checking                */
        uint32_t ALERT      : 1;  /* Bit 13: SMBus Alert                          */
        uint32_t RESERVED1  : 1;  /* Bit 14: Reserved                             */
        uint32_t SWRST      : 1;  /* Bit 15: Software Reset                       */
        uint32_t RESERVED2  : 16; /* Bits 31:16 Reserved                          */
    } BITS;
} I2C_CR1_t;


/**
 * ============================================================================
 * I2C_CR2 - Control Register 2
 * Address offset: 0x04
 * Reset value: 0x0000
 * ============================================================================
 */
typedef union {
    volatile uint32_t ALL;
    struct {
        uint32_t FREQ       : 6;  /* Bits 5:0:  Peripheral Clock Frequency (2-50 MHz) */
        uint32_t RESERVED0  : 2;  /* Bits 7:6:  Reserved                              */
        uint32_t ITERREN    : 1;  /* Bit 8:     Error Interrupt Enable                 */
        uint32_t ITEVTEN    : 1;  /* Bit 9:     Event Interrupt Enable                 */
        uint32_t ITBUFEN    : 1;  /* Bit 10:    Buffer Interrupt Enable                */
        uint32_t DMAEN      : 1;  /* Bit 11:    DMA Requests Enable                   */
        uint32_t LAST       : 1;  /* Bit 12:    DMA Last Transfer                     */
        uint32_t RESERVED1  : 19; /* Bits 31:13 Reserved                              */
    } BITS;
} I2C_CR2_t;


/**
 * ============================================================================
 * I2C_OAR1 - Own Address Register 1
 * Address offset: 0x08
 * Reset value: 0x0000
 * ============================================================================
 */
typedef union {
    volatile uint32_t ALL;
    struct {
        uint32_t ADD0       : 1;  /* Bit 0:     Address bit 0 (10-bit mode)       */
        uint32_t ADD7_1     : 7;  /* Bits 7:1:  Address bits 7:1                  */
        uint32_t ADD9_8     : 2;  /* Bits 9:8:  Address bits 9:8 (10-bit mode)    */
        uint32_t RESERVED0  : 4;  /* Bits 13:10 Reserved, keep at reset value     */
        uint32_t SHOULDBE1  : 1;  /* Bit 14:    Must be kept at 1 by software     */
        uint32_t ADDMODE    : 1;  /* Bit 15:    0 = 7-bit, 1 = 10-bit addressing  */
        uint32_t RESERVED1  : 16; /* Bits 31:16 Reserved                          */
    } BITS;
} I2C_OAR1_t;


/**
 * ============================================================================
 * I2C_OAR2 - Own Address Register 2
 * Address offset: 0x0C
 * Reset value: 0x0000
 * ============================================================================
 */
typedef union {
    volatile uint32_t ALL;
    struct {
        uint32_t ENDUAL     : 1;  /* Bit 0:    Dual Addressing Mode Enable        */
        uint32_t ADD2       : 7;  /* Bits 7:1: Secondary interface address         */
        uint32_t RESERVED0  : 24; /* Bits 31:8 Reserved                           */
    } BITS;
} I2C_OAR2_t;


/**
 * ============================================================================
 * I2C_DR - Data Register
 * Address offset: 0x10
 * Reset value: 0x0000
 * ============================================================================
 */
typedef union {
    volatile uint32_t ALL;
    struct {
        uint32_t DR         : 8;  /* Bits 7:0: 8-bit Data Register                */
        uint32_t RESERVED0  : 24; /* Bits 31:8 Reserved                           */
    } BITS;
} I2C_DR_t;


/**
 * ============================================================================
 * I2C_SR1 - Status Register 1
 * Address offset: 0x14
 * Reset value: 0x0000
 * ============================================================================
 */
typedef union {
    volatile uint32_t ALL;
    struct {
        uint32_t SB         : 1;  /* Bit 0:  Start Bit (Master mode)              */
        uint32_t ADDR       : 1;  /* Bit 1:  Address Sent / Matched               */
        uint32_t BTF        : 1;  /* Bit 2:  Byte Transfer Finished               */
        uint32_t ADD10      : 1;  /* Bit 3:  10-bit Header Sent (Master)          */
        uint32_t STOPF      : 1;  /* Bit 4:  Stop Detection (Slave mode)          */
        uint32_t RESERVED0  : 1;  /* Bit 5:  Reserved                             */
        uint32_t RXNE       : 1;  /* Bit 6:  Data Register Not Empty              */
        uint32_t TXE        : 1;  /* Bit 7:  Data Register Empty                  */
        uint32_t BERR       : 1;  /* Bit 8:  Bus Error                            */
        uint32_t ARLO       : 1;  /* Bit 9:  Arbitration Lost (Master)            */
        uint32_t AF         : 1;  /* Bit 10: Acknowledge Failure                  */
        uint32_t OVR        : 1;  /* Bit 11: Overrun/Underrun                     */
        uint32_t PECERR     : 1;  /* Bit 12: PEC Error in Reception               */
        uint32_t RESERVED1  : 1;  /* Bit 13: Reserved                             */
        uint32_t TIMEOUT    : 1;  /* Bit 14: Timeout or Tlow Error (SMBus)        */
        uint32_t SMBALERT   : 1;  /* Bit 15: SMBus Alert                          */
        uint32_t RESERVED2  : 16; /* Bits 31:16 Reserved                          */
    } BITS;
} I2C_SR1_t;


/**
 * ============================================================================
 * I2C_SR2 - Status Register 2
 * Address offset: 0x18
 * Reset value: 0x0000
 * ============================================================================
 */
typedef union {
    volatile uint32_t ALL;
    struct {
        uint32_t MSL        : 1;  /* Bit 0:    Master/Slave                       */
        uint32_t BUSY       : 1;  /* Bit 1:    Bus Busy                           */
        uint32_t TRA        : 1;  /* Bit 2:    Transmitter/Receiver               */
        uint32_t RESERVED0  : 1;  /* Bit 3:    Reserved                           */
        uint32_t GENCALL    : 1;  /* Bit 4:    General Call Address (Slave)        */
        uint32_t SMBDEFAULT : 1;  /* Bit 5:    SMBus Device Default Address       */
        uint32_t SMBHOST    : 1;  /* Bit 6:    SMBus Host Header (Slave)          */
        uint32_t DUALF      : 1;  /* Bit 7:    Dual Flag (Slave)                  */
        uint32_t PEC        : 8;  /* Bits 15:8 Packet Error Checking Register     */
        uint32_t RESERVED1  : 16; /* Bits 31:16 Reserved                          */
    } BITS;
} I2C_SR2_t;


/**
 * ============================================================================
 * I2C_CCR - Clock Control Register
 * Address offset: 0x1C
 * Reset value: 0x0000
 * ============================================================================
 *
 * CCR Calculation Formulas:
 * -------------------------
 * Standard Mode (Sm): CCR = PCLK1 / (2 * SCL_frequency)
 *   Example: PCLK1=42MHz, SCL=100KHz -> CCR = 210
 *
 * Fast Mode (Fm) DUTY=0: CCR = PCLK1 / (3 * SCL_frequency)
 *   Example: PCLK1=42MHz, SCL=400KHz -> CCR = 35
 *
 * Fast Mode (Fm) DUTY=1: CCR = PCLK1 / (25 * SCL_frequency)
 * ============================================================================
 */
typedef union {
    volatile uint32_t ALL;
    struct {
        uint32_t CCR        : 12; /* Bits 11:0: Clock Control (Master mode)       */
        uint32_t RESERVED0  : 2;  /* Bits 13:12 Reserved                          */
        uint32_t DUTY       : 1;  /* Bit 14:    Fm Duty Cycle (0 = 2:1, 1 = 16/9)*/
        uint32_t FS         : 1;  /* Bit 15:    0 = Sm mode, 1 = Fm mode          */
        uint32_t RESERVED1  : 16; /* Bits 31:16 Reserved                          */
    } BITS;
} I2C_CCR_t;


/**
 * ============================================================================
 * I2C_TRISE - TRISE Register
 * Address offset: 0x20
 * Reset value: 0x0002
 * ============================================================================
 *
 * TRISE = (max_rise_time_ns * PCLK1_MHz / 1000) + 1
 *   Sm: 1000 ns -> PCLK1=42MHz -> TRISE = 43
 *   Fm:  300 ns -> PCLK1=42MHz -> TRISE = 13
 * ============================================================================
 */
typedef union {
    volatile uint32_t ALL;
    struct {
        uint32_t TRISE      : 6;  /* Bits 5:0: Maximum Rise Time (Master mode)    */
        uint32_t RESERVED0  : 26; /* Bits 31:6 Reserved                           */
    } BITS;
} I2C_TRISE_t;


/**
 * ============================================================================
 * I2C_FLTR - Filter Register
 * Address offset: 0x24
 * Reset value: 0x0000
 * ============================================================================
 */
typedef union {
    volatile uint32_t ALL;
    struct {
        uint32_t DNF        : 4;  /* Bits 3:0: Digital Noise Filter (0 = off)     */
        uint32_t ANOFF      : 1;  /* Bit 4:    Analog Noise Filter OFF            */
        uint32_t RESERVED0  : 27; /* Bits 31:5 Reserved                           */
    } BITS;
} I2C_FLTR_t;


/**
 * ============================================================================
 * Complete I2C Register Block Structure
 * ============================================================================
 */
typedef struct {
    I2C_CR1_t   CR1;    /* 0x00: Control register 1          */
    I2C_CR2_t   CR2;    /* 0x04: Control register 2          */
    I2C_OAR1_t  OAR1;   /* 0x08: Own address register 1      */
    I2C_OAR2_t  OAR2;   /* 0x0C: Own address register 2      */
    I2C_DR_t    DR;     /* 0x10: Data register                */
    I2C_SR1_t   SR1;    /* 0x14: Status register 1            */
    I2C_SR2_t   SR2;    /* 0x18: Status register 2            */
    I2C_CCR_t   CCR;    /* 0x1C: Clock control register       */
    I2C_TRISE_t TRISE;  /* 0x20: TRISE register               */
    I2C_FLTR_t  FLTR;   /* 0x24: Filter register              */
} I2C_REGS_t;


/**
 * ============================================================================
 * I2C Peripheral Base Address Definitions
 * ============================================================================
 */

/* I2C-R02: UL suffix on all base-address constants (MISRA Rule 7.2) */
#define I2C1_BASEADDR   0x40005400UL
#define I2C2_BASEADDR   0x40005800UL
#define I2C3_BASEADDR   0x40005C00UL

/* I2C-R03: Cast to volatile I2C_REGS_t* so accesses are not optimised away */
#define I2C1            ((volatile I2C_REGS_t *) I2C1_BASEADDR)
#define I2C2            ((volatile I2C_REGS_t *) I2C2_BASEADDR)
#define I2C3            ((volatile I2C_REGS_t *) I2C3_BASEADDR)


/*===========================================================================*/
/*                          PRIVATE MACROS                                   */
/*===========================================================================*/

/* Timeout Values */
#define I2C_TIMEOUT_FLAG            50000U
#define I2C_TIMEOUT_BUSY            50000U

/* SR1 Flag Bit Positions */
#define I2C_SR1_SB_POS              0U
#define I2C_SR1_ADDR_POS            1U
#define I2C_SR1_BTF_POS             2U
#define I2C_SR1_ADD10_POS           3U
#define I2C_SR1_STOPF_POS           4U
#define I2C_SR1_RXNE_POS            6U
#define I2C_SR1_TXE_POS             7U
#define I2C_SR1_BERR_POS            8U
#define I2C_SR1_ARLO_POS            9U
#define I2C_SR1_AF_POS              10U
#define I2C_SR1_OVR_POS             11U

/* SR2 Flag Bit Positions */
#define I2C_SR2_MSL_POS             0U
#define I2C_SR2_BUSY_POS            1U
#define I2C_SR2_TRA_POS             2U

/* Direction */
#define I2C_DIRECTION_TX            0U
#define I2C_DIRECTION_RX            1U

/* Speed Values in Hz */
#define I2C_SPEED_SM_HZ             100000U     /* 100 kHz  Standard Mode    */
#define I2C_SPEED_FM_HZ             400000U     /* 400 kHz  Fast Mode        */
/* I2C-R05: STM32F401CC does NOT support Fast Mode Plus — do not use */
#define I2C_SPEED_FMP_HZ            1000000U    /* 1 MHz — NOT supported on STM32F401CC */

/* Rise Time in ns */
#define I2C_RISE_TIME_SM_NS         1000U       /* 1000 ns for SM */
#define I2C_RISE_TIME_FM_NS         300U        /* 300 ns for FM  */

/* 10-bit Address Header */
#define I2C_10BIT_HEADER_BASE       0xF0U       /* 11110xx0 */

/* SR1 Flag Bit Masks */
#define I2C_SR1_SB_MASK             0x0001U
#define I2C_SR1_ADDR_MASK           0x0002U
#define I2C_SR1_BTF_MASK            0x0004U
#define I2C_SR1_ADD10_MASK          0x0008U
#define I2C_SR1_STOPF_MASK          0x0010U
#define I2C_SR1_RXNE_MASK           0x0040U
#define I2C_SR1_TXE_MASK            0x0080U
#define I2C_SR1_BERR_MASK           0x0100U
#define I2C_SR1_ARLO_MASK           0x0200U
#define I2C_SR1_AF_MASK             0x0400U
#define I2C_SR1_OVR_MASK            0x0800U

/* Combined Error Mask */
#define I2C_SR1_ERROR_MASK          (I2C_SR1_BERR_MASK | I2C_SR1_ARLO_MASK | \
                                     I2C_SR1_AF_MASK   | I2C_SR1_OVR_MASK)

/* SR2 Flag Bit Masks */
#define I2C_SR2_MSL_MASK            0x0001U
#define I2C_SR2_BUSY_MASK           0x0002U
#define I2C_SR2_TRA_MASK            0x0004U

#endif /* I2C_REGS_H */