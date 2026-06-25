#ifndef FLASH_REGS_H
#define FLASH_REGS_H

#include "STD_TYPES.h"

/* Base address for Flash interface (RM0368 Section 3.3) */
#define FLASH_BASE_ADDR   0x40023C00UL

/* Flash unlock keys */
#define FLASH_KEY1        0x45670123UL
#define FLASH_KEY2        0xCDEF89ABUL

/* Flash option bytes unlock keys */
#define FLASH_OPTKEY1     0x08192A3BUL
#define FLASH_OPTKEY2     0x4C5D6E7FUL

/* =======================
 *   Timeout Values
 * ======================= */
#define FLASH_TIMEOUT_DEFAULT    50000UL   /* Default timeout for operations */
#define FLASH_TIMEOUT_ERASE      100000UL  /* Timeout for erase operations   */

/* =======================
 *   Flash Memory Boundaries
 * ======================= */
#define FLASH_START_ADDRESS      0x08000000UL
#define FLASH_END_ADDRESS        0x0803FFFFUL  /* 256 KB total */
#define FLASH_SIZE               (256UL * 1024UL)
/* =======================
 *   Flash register layout
 * ======================= */
typedef struct
{
    /* 0x00: Flash access control register (FLASH_ACR) */
    union {
        volatile uint32_t ALL;
        struct {
            volatile uint32_t LATENCY  : 3;  /* Bits 2:0  Latency (wait states)      */
            volatile uint32_t RES0     : 5;  /* Bits 7:3  Reserved                   */
            volatile uint32_t PRFTEN   : 1;  /* Bit  8    Prefetch enable            */
            volatile uint32_t ICEN     : 1;  /* Bit  9    Instruction cache enable   */
            volatile uint32_t DCEN     : 1;  /* Bit  10   Data cache enable          */
            volatile uint32_t ICRST    : 1;  /* Bit  11   Instruction cache reset    */
            volatile uint32_t DCRST    : 1;  /* Bit  12   Data cache reset           */
            volatile uint32_t RES1     : 19; /* Bits 31:13 Reserved                  */
        } BITS;
    } ACR;

    /* 0x04: Flash key register (FLASH_KEYR) - Write only */
    union {
        volatile uint32_t ALL;
        struct {
            volatile uint32_t KEY : 32;  /* Bits 31:0  Flash unlock key */
        } BITS;
    } KEYR;

    /* 0x08: Flash option key register (FLASH_OPTKEYR) - Write only */
    union {
        volatile uint32_t ALL;
        struct {
            volatile uint32_t OPTKEY : 32;  /* Bits 31:0  Option byte unlock key */
        } BITS;
    } OPTKEYR;

    /* 0x0C: Flash status register (FLASH_SR) */
    union {
        volatile uint32_t ALL;
        struct {
            volatile uint32_t EOP      : 1;  /* Bit  0    End of operation           */
            volatile uint32_t OPERR    : 1;  /* Bit  1    Operation error            */
            volatile uint32_t RES0     : 2;  /* Bits 3:2  Reserved                   */
            volatile uint32_t WRPERR   : 1;  /* Bit  4    Write protection error     */
            volatile uint32_t PGAERR   : 1;  /* Bit  5    Programming alignment err  */
            volatile uint32_t PGPERR   : 1;  /* Bit  6    Programming parallelism err*/
            volatile uint32_t PGSERR   : 1;  /* Bit  7    Programming sequence error */
            volatile uint32_t RES1     : 8;  /* Bits 15:8 Reserved                   */
            volatile uint32_t BSY      : 1;  /* Bit  16   Busy                       */
            volatile uint32_t RES2     : 15; /* Bits 31:17 Reserved                  */
        } BITS;
    } SR;

    /* 0x10: Flash control register (FLASH_CR) */
    union {
        volatile uint32_t ALL;
        struct {
            volatile uint32_t PG       : 1;  /* Bit  0    Programming                */
            volatile uint32_t SER      : 1;  /* Bit  1    Sector erase               */
            volatile uint32_t MER      : 1;  /* Bit  2    Mass erase                 */
            volatile uint32_t SNB      : 4;  /* Bits 6:3  Sector number (0-5 for F401) */
            volatile uint32_t RES0     : 1;  /* Bit  7    Reserved                   */
            volatile uint32_t PSIZE    : 2;  /* Bits 9:8  Program size               */
            volatile uint32_t RES1     : 6;  /* Bits 15:10 Reserved                  */
            volatile uint32_t STRT     : 1;  /* Bit  16   Start erase operation      */
            volatile uint32_t RES2     : 7;  /* Bits 23:17 Reserved                  */
            volatile uint32_t EOPIE    : 1;  /* Bit  24   End of operation interrupt */
            volatile uint32_t ERRIE    : 1;  /* Bit  25   Error interrupt enable     */
            volatile uint32_t RES3     : 5;  /* Bits 30:26 Reserved                  */
            volatile uint32_t LOCK     : 1;  /* Bit  31   Lock Flash control register*/
        } BITS;
    } CR;

    /* 0x14: Flash option control register (FLASH_OPTCR) */
    union {
        volatile uint32_t ALL;
        struct {
            volatile uint32_t OPTLOCK  : 1;  /* Bit  0    Option lock                */
            volatile uint32_t OPTSTRT  : 1;  /* Bit  1    Option start               */
            volatile uint32_t BOR_LEV  : 2;  /* Bits 3:2  BOR reset level            */
            volatile uint32_t RES0     : 1;  /* Bit  4    Reserved                   */
            volatile uint32_t WDG_SW   : 1;  /* Bit  5    Watchdog SW select         */
            volatile uint32_t nRST_STOP: 1;  /* Bit  6    Reset on STOP mode         */
            volatile uint32_t nRST_STDBY:1;  /* Bit  7    Reset on Standby mode      */
            volatile uint32_t RDP      : 8;  /* Bits 15:8 Read protection level      */
            volatile uint32_t nWRP     : 12; /* Bits 27:16 Not write protect (F401: only bits 21:16 used) */
            volatile uint32_t RES1     : 4;  /* Bits 31:28 Reserved                  */
        } BITS;
    } OPTCR;

} FLASH_REGS_t;

/* =======================
 *   Bit position defines
 * ======================= */

/* FLASH_ACR bits */
#define FLASH_ACR_LATENCY_POS   0U
#define FLASH_ACR_PRFTEN_POS    8U
#define FLASH_ACR_ICEN_POS      9U
#define FLASH_ACR_DCEN_POS      10U
#define FLASH_ACR_ICRST_POS     11U
#define FLASH_ACR_DCRST_POS     12U

/* FLASH_SR bits */
#define FLASH_SR_EOP_POS        0U
#define FLASH_SR_OPERR_POS      1U
#define FLASH_SR_WRPERR_POS     4U
#define FLASH_SR_PGAERR_POS     5U
#define FLASH_SR_PGPERR_POS     6U
#define FLASH_SR_PGSERR_POS     7U
#define FLASH_SR_BSY_POS        16U

/* FLASH_CR bits */
#define FLASH_CR_PG_POS         0U
#define FLASH_CR_SER_POS        1U
#define FLASH_CR_MER_POS        2U
#define FLASH_CR_SNB_POS        3U
#define FLASH_CR_PSIZE_POS      8U
#define FLASH_CR_STRT_POS       16U
#define FLASH_CR_EOPIE_POS      24U
#define FLASH_CR_ERRIE_POS      25U
#define FLASH_CR_LOCK_POS       31U

/* =======================
 *   Bit masks
 * ======================= */

/* FLASH_ACR masks */
#define FLASH_ACR_LATENCY       (0x7UL << FLASH_ACR_LATENCY_POS)
#define FLASH_ACR_PRFTEN        (1UL << FLASH_ACR_PRFTEN_POS)
#define FLASH_ACR_ICEN          (1UL << FLASH_ACR_ICEN_POS)
#define FLASH_ACR_DCEN          (1UL << FLASH_ACR_DCEN_POS)
#define FLASH_ACR_ICRST         (1UL << FLASH_ACR_ICRST_POS)
#define FLASH_ACR_DCRST         (1UL << FLASH_ACR_DCRST_POS)

/* FLASH_SR masks */
#define FLASH_SR_EOP            (1UL << FLASH_SR_EOP_POS)
#define FLASH_SR_OPERR          (1UL << FLASH_SR_OPERR_POS)
#define FLASH_SR_WRPERR         (1UL << FLASH_SR_WRPERR_POS)
#define FLASH_SR_PGAERR         (1UL << FLASH_SR_PGAERR_POS)
#define FLASH_SR_PGPERR         (1UL << FLASH_SR_PGPERR_POS)
#define FLASH_SR_PGSERR         (1UL << FLASH_SR_PGSERR_POS)
#define FLASH_SR_BSY            (1UL << FLASH_SR_BSY_POS)

/* All error flags combined for easy clearing */
#define FLASH_SR_ERROR_MASK     (FLASH_SR_OPERR | FLASH_SR_WRPERR | \
                                 FLASH_SR_PGAERR | FLASH_SR_PGPERR | \
                                 FLASH_SR_PGSERR)

/* FLASH_CR masks */
#define FLASH_CR_PG             (1UL << FLASH_CR_PG_POS)
#define FLASH_CR_SER            (1UL << FLASH_CR_SER_POS)
#define FLASH_CR_MER            (1UL << FLASH_CR_MER_POS)
#define FLASH_CR_SNB            (0xFUL << FLASH_CR_SNB_POS)
#define FLASH_CR_PSIZE          (0x3UL << FLASH_CR_PSIZE_POS)
#define FLASH_CR_STRT           (1UL << FLASH_CR_STRT_POS)
#define FLASH_CR_EOPIE          (1UL << FLASH_CR_EOPIE_POS)
#define FLASH_CR_ERRIE          (1UL << FLASH_CR_ERRIE_POS)
#define FLASH_CR_LOCK           (1UL << FLASH_CR_LOCK_POS)

/* =======================
 *   PSIZE values for CR register
 * ======================= */
#define FLASH_PSIZE_BYTE        0x00UL  /* Program x8  (byte)        */
#define FLASH_PSIZE_HALF_WORD   0x01UL  /* Program x16 (half-word)   */
#define FLASH_PSIZE_WORD        0x02UL  /* Program x32 (word)        */
#define FLASH_PSIZE_DOUBLE_WORD 0x03UL  /* Program x64 (double-word) */

/* =======================
 *   Latency values (based on CPU frequency and VDD)
 * ======================= */
/* For STM32F401 with VDD = 2.7V to 3.6V:
 *   0 WS: 0  < HCLK ≤ 30 MHz
 *   1 WS: 30 < HCLK ≤ 60 MHz
 *   2 WS: 60 < HCLK ≤ 84 MHz
 */
#define FLASH_LATENCY_0WS       0x00UL
#define FLASH_LATENCY_1WS       0x01UL
#define FLASH_LATENCY_2WS       0x02UL

/* =======================
 *   Sector information for STM32F401xC (256 KB)
 * ======================= */
// #define FLASH_SECTOR_0          0x00UL  /* 16 KB  0x0800 0000 - 0x0800 3FFF */
// #define FLASH_SECTOR_1          0x01UL  /* 16 KB  0x0800 4000 - 0x0800 7FFF */
// #define FLASH_SECTOR_2          0x02UL  /* 16 KB  0x0800 8000 - 0x0800 BFFF */
// #define FLASH_SECTOR_3          0x03UL  /* 16 KB  0x0800 C000 - 0x0800 FFFF */
// #define FLASH_SECTOR_4          0x04UL  /* 64 KB  0x0801 0000 - 0x0801 FFFF */
// #define FLASH_SECTOR_5          0x05UL  /* 128 KB 0x0802 0000 - 0x0803 FFFF */

/* Sector base addresses */
#define FLASH_SECTOR_0_BASE     0x08000000UL
#define FLASH_SECTOR_1_BASE     0x08004000UL
#define FLASH_SECTOR_2_BASE     0x08008000UL
#define FLASH_SECTOR_3_BASE     0x0800C000UL
#define FLASH_SECTOR_4_BASE     0x08010000UL
#define FLASH_SECTOR_5_BASE     0x08020000UL

/* Sector sizes */
#define FLASH_SECTOR_0_SIZE     (16UL * 1024UL)   /* 16 KB */
#define FLASH_SECTOR_1_SIZE     (16UL * 1024UL)   /* 16 KB */
#define FLASH_SECTOR_2_SIZE     (16UL * 1024UL)   /* 16 KB */
#define FLASH_SECTOR_3_SIZE     (16UL * 1024UL)   /* 16 KB */
#define FLASH_SECTOR_4_SIZE     (64UL * 1024UL)   /* 64 KB */
#define FLASH_SECTOR_5_SIZE     (128UL * 1024UL)  /* 128 KB */

/* Total Flash size */
#define FLASH_TOTAL_SIZE        (256UL * 1024UL)  /* 256 KB */

/* Number of sectors */
#define FLASH_SECTOR_COUNT      6U

/* Handy pointer */
#define FLASH   ((FLASH_REGS_t *)FLASH_BASE_ADDR)

#endif /* FLASH_REGS_H */