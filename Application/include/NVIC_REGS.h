#ifndef NVIC_REGS_H
#define NVIC_REGS_H

#include "STD_TYPES.h"

#define NVIC_BASE_ADDR 0xE000E100UL  /* N-01: UL suffix added — MISRA Rule 7.2 requires UL on address literals */
#define SCB_BASE_ADDR  0xE000ED00UL  /* N-01: UL suffix added — MISRA Rule 7.2 requires UL on address literals */

#define NVIC ((volatile NVIC_REGS_t *)NVIC_BASE_ADDR)
#define SCB ((volatile SCB_REGS_t *)SCB_BASE_ADDR)

#define ISER_REGS_NUM 8
#define ICER_REG_NUM 8

#define NVIC_MAX_IRQ_NUM  84U  /* N-11: STM32F401CC maximum external IRQ number (SPI4); replaces magic 239U */

#define IRQ_MASK 0xFFU       // 8 bits
#define REG_INDEX_MASK 0x07U // 3 bits
#define BIT_POS_MASK 0x1FU   // 5 bits

#define IRQ_SHIFT 0U
#define REG_INDEX_SHIFT 8U
#define BIT_POS_SHIFT 11U

#define GET_IRQ_NUM(encoded)   (((encoded) >> IRQ_SHIFT)       & IRQ_MASK)       /* N-03: parens around (encoded) prevent operator-precedence hazard */
#define GET_REG_INDEX(encoded) (((encoded) >> REG_INDEX_SHIFT) & REG_INDEX_MASK) /* N-03: parens around (encoded) prevent operator-precedence hazard */
#define GET_BIT_POS(encoded)   (((encoded) >> BIT_POS_SHIFT)   & BIT_POS_MASK)   /* N-03: parens around (encoded) prevent operator-precedence hazard */

#define AIRCR_VECTKEY          (0x5FAU << 16U)   /* N-06: U suffix on shift amount added — MISRA Rule 7.2 */
#define AIRCR_VECTKEY_MASK     (0xFFFF0000U)
#define AIRCR_PRIGROUP_MASK    (0x00000700U)

#define AIRCR_SYSRESETREQ      (1U << 2)

typedef struct
{
    /* ============================================================
     * 0x000 – 0x01C : ISER (Interrupt Set-Enable Registers)
     * ============================================================ */
    union
    {
        volatile uint32_t ISER_REG[8];
        struct
        {
            volatile uint32_t ISER0; // 0x000
            volatile uint32_t ISER1; // 0x004
            volatile uint32_t ISER2; // 0x008
            volatile uint32_t ISER3; // 0x00C
            volatile uint32_t ISER4; // 0x010
            volatile uint32_t ISER5; // 0x014
            volatile uint32_t ISER6; // 0x018
            volatile uint32_t ISER7; // 0x01C
        };
    } ISER;

    volatile uint32_t RESERVED0[24]; /* N-04: volatile added — padding must be volatile to prevent optimiser removing accesses */

    /* ============================================================
     * 0x080 – 0x09C : ICER (Interrupt Clear-Enable Registers)
     * ============================================================ */
    union
    {
        volatile uint32_t ICER_REG[8];
        struct
        {
            volatile uint32_t ICER0; // 0x080
            volatile uint32_t ICER1; // 0x084
            volatile uint32_t ICER2; // 0x088
            volatile uint32_t ICER3; // 0x08C
            volatile uint32_t ICER4; // 0x090
            volatile uint32_t ICER5; // 0x094
            volatile uint32_t ICER6; // 0x098
            volatile uint32_t ICER7; // 0x09C
        };
    } ICER;

    volatile uint32_t RESERVED1[24]; /* N-04: volatile added — padding must be volatile to prevent optimiser removing accesses */

    /* ============================================================
     * 0x100 – 0x11C : ISPR (Interrupt Set-Pending Registers)
     * ============================================================ */
    union
    {
        volatile uint32_t ISPR_REG[8];
        struct
        {
            volatile uint32_t ISPR0; // 0x100
            volatile uint32_t ISPR1; // 0x104
            volatile uint32_t ISPR2; // 0x108
            volatile uint32_t ISPR3; // 0x10C
            volatile uint32_t ISPR4; // 0x110
            volatile uint32_t ISPR5; // 0x114
            volatile uint32_t ISPR6; // 0x118
            volatile uint32_t ISPR7; // 0x11C
        };
    } ISPR;

    volatile uint32_t RESERVED2[24]; /* N-04: volatile added — padding must be volatile to prevent optimiser removing accesses */

    /* ============================================================
     * 0x180 – 0x19C : ICPR (Interrupt Clear-Pending Registers)
     * ============================================================ */
    union
    {
        volatile uint32_t ICPR_REG[8];
        struct
        {
            volatile uint32_t ICPR0; // 0x180
            volatile uint32_t ICPR1; // 0x184
            volatile uint32_t ICPR2; // 0x188
            volatile uint32_t ICPR3; // 0x18C
            volatile uint32_t ICPR4; // 0x190
            volatile uint32_t ICPR5; // 0x194
            volatile uint32_t ICPR6; // 0x198
            volatile uint32_t ICPR7; // 0x19C
        };
    } ICPR;

    volatile uint32_t RESERVED3[24]; /* N-04: volatile added — padding must be volatile to prevent optimiser removing accesses */

    /* ============================================================
     * 0x200 – 0x21C : IABR (Interrupt Active Bit Registers)
     * ============================================================ */
    union
    {
        volatile uint32_t IABR_REG[8];
        struct
        {
            volatile uint32_t IABR0; // 0x200
            volatile uint32_t IABR1; // 0x204
            volatile uint32_t IABR2; // 0x208
            volatile uint32_t IABR3; // 0x20C
            volatile uint32_t IABR4; // 0x210
            volatile uint32_t IABR5; // 0x214
            volatile uint32_t IABR6; // 0x218
            volatile uint32_t IABR7; // 0x21C
        };
    } IABR;

    volatile uint32_t RESERVED4[56]; /* N-04: volatile added — padding must be volatile to prevent optimiser removing accesses */

    /* ============================================================
     * 0x300 – 0x4EC : IPR0–IPR59 (Interrupt Priority Registers)
     * ============================================================ */
    union
    {
        volatile uint8_t IPR_BYTE[240]; // Byte access for 240 interrupts
        volatile uint32_t IPR_REG[60];  // Word access (60 x 32-bit)

    } IPR;

    volatile uint32_t RESERVED5[644]; /* N-04: volatile added — gap 0x3F0→0xE00 = 0xA10 = 644 uint32_t words; N-07: size verified correct, no change */

    /* ============================================================
     * 0xE00 : STIR (Software Trigger Interrupt Register)
     * ============================================================ */
    volatile uint32_t STIR; // 0xE00

} NVIC_REGS_t;

typedef struct
{
    /* ============================================================
     * 0x000 : CPUID
     * ============================================================ */
    union
    {
        volatile uint32_t ALL;
        struct
        {
            uint32_t REVISION : 4;    /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t PARTNO : 12;     /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t CONSTANT : 4;    /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t VARIANT : 4;     /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t IMPLEMENTER : 8; /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
        } BITS;
    } CPUID;

    /* ============================================================
     * 0x004 : ICSR
     * ============================================================ */
    union
    {
        volatile uint32_t ALL;
        struct
        {
            uint32_t VECTACTIVE : 9;  /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t RESERVED0 : 2;   /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t RETTOBASE : 1;   /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t VECTPENDING : 7; /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t RESERVED2 : 3;   /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t ISRPENDING : 1;  /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t RESERVED3 : 2;   /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t PENDSTCLR : 1;   /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t PENDSTSET : 1;   /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t PENDSVCLR : 1;   /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t PENDSVSET : 1;   /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t RESERVED4 : 2;   /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t NMIPENDSET : 1;  /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
        } BITS;
    } ICSR;

    /* ============================================================
     * 0x008 : VTOR
     * ============================================================ */
    union
    {
        volatile uint32_t ALL;
        struct
        {
            uint32_t TBLOFF : 25;  /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t RESERVED : 7; /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
        } BITS;
    } VTOR;

    /* ============================================================
     * 0x00C : AIRCR
     * ============================================================ */
    union
    {
        volatile uint32_t ALL;
        struct
        {
            uint32_t VECTRESET : 1;     /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t VECTCLRACTIVE : 1; /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t SYSRESETREQ : 1;   /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t RESERVED0 : 5;     /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t PRIGROUP : 3;      /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t RESERVED1 : 4;     /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t ENDIANNESS : 1;    /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t VECTKEY : 16;      /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
        } BITS;
    } AIRCR;

    /* ============================================================
     * 0x010 : SCR
     * ============================================================ */
    union
    {
        volatile uint32_t ALL;
        struct
        {
            uint32_t RESERVED0 : 1;    /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t SLEEPONEXIT : 1;  /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t SLEEPDEEP : 1;    /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t RESERVED1 : 1;    /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t SEVONPEND : 1;    /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t RESERVED2 : 27;   /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
        } BITS;
    } SCR;

    /* ============================================================
     * 0x014 : CCR
     * ============================================================ */
    union
    {
        volatile uint32_t ALL;
        struct
        {
            uint32_t NONBASETHRDENA : 1; /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t USERSETMPEND : 1;   /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t RESERVED0 : 1;      /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t UNALIGN_TRP : 1;    /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t DIV_0_TRP : 1;      /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t RESERVED1 : 3;      /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t STKALIGN : 1;       /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t RESERVED2 : 22;     /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
        } BITS;
    } CCR;

    /* ============================================================
     * 0x018 : SHPR1
     * ============================================================ */
    union
    {
        volatile uint32_t ALL;
        struct
        {
            uint32_t PRI4 : 8; /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t PRI5 : 8; /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t PRI6 : 8; /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t RSVD : 8; /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
        } BITS;
    } SHPR1;

    /* ============================================================
     * 0x01C : SHPR2
     * ============================================================ */
    union
    {
        volatile uint32_t ALL;
        struct
        {
            uint32_t PRI11 : 8;  /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t RSVD : 24;  /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
        } BITS;
    } SHPR2;

    /* ============================================================
     * 0x020 : SHPR3
     * ============================================================ */
    union
    {
        volatile uint32_t ALL;
        struct
        {
            uint32_t PRI14 : 8; /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t PRI15 : 8; /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t RSVD : 16; /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
        } BITS;
    } SHPR3;

    /* ============================================================
     * 0x024 : SHCSR
     * ============================================================ */
    union
    {
        volatile uint32_t ALL;
        struct
        {
            uint32_t MEMFAULTACT : 1;     /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t BUSFAULTACT : 1;     /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t USGFAULTACT : 1;     /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t RESERVED0 : 5;       /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t SVCALLACT : 1;       /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t RESERVED1 : 1;       /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t MONITORACT : 1;      /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t RESERVED2 : 1;       /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t PENDSVACT : 1;       /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t SYSTICKACT : 1;      /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t USGFAULTPENDED : 1;  /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t MEMFAULTPENDED : 1;  /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t BUSFAULTPENDED : 1;  /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t SVCALLPENDED : 1;    /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t RESERVED3 : 1;       /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t MEMFAULTENA : 1;     /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t BUSFAULTENA : 1;     /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t USGFAULTENA : 1;     /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
        } BITS;
    } SHCSR;

    /* ============================================================
     * 0x028 : CFSR
     * ============================================================ */
    union
    {
        volatile uint32_t ALL;
        struct
        {
            uint32_t MMFSR : 8;  /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t BFSR : 8;   /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t UFSR : 16;  /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
        } BITS;
    } CFSR;

    /* ============================================================
     * 0x02C : HFSR
     * ============================================================ */
    union
    {
        volatile uint32_t ALL;
        struct
        {
            uint32_t RESERVED : 1;   /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t VECTTBL : 1;    /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t RESERVED2 : 28; /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t FORCED : 1;     /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t DEBUGEVT : 1;   /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
        } BITS;
    } HFSR;

    /* ============================================================
     * 0x030 : DFSR
     * ============================================================ */
    union
    {
        volatile uint32_t ALL;
        struct
        {
            uint32_t HALTED : 1;    /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t BKPT : 1;      /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t DWTTRAP : 1;   /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t VCATCH : 1;    /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t EXTERNAL : 1;  /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
            uint32_t RESERVED : 27; /* N-05: volatile removed from bitfield — non-standard per C99/C11; volatile is on ALL */
        } BITS;
    } DFSR;

    /* ============================================================
     * 0x034 MMFAR, 0x038 BFAR, 0x03C AFSR (fault addresses)
     * ============================================================ */
    volatile uint32_t MMFAR;
    volatile uint32_t BFAR;
    volatile uint32_t AFSR;

} SCB_REGS_t;

#endif
