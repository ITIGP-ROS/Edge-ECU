#ifndef DMA_REGS_H_
#define DMA_REGS_H_
#include "STD_TYPES.h"



#define DMA1_BASE_ADDR 0x40026000UL
#define DMA2_BASE_ADDR 0x40026400UL


/* D-01 WARNING: The BITS struct below does NOT accurately reflect the hardware register layout.
   The actual layout has non-uniform gaps between stream flag groups (RM0368 Table 42).
   Do NOT use BITS struct members directly — always use DMA_ReadISR() with the ALL
   register and the lookup table masks in DMA.c for correct flag access. */
typedef union {
    volatile uint32_t ALL;
    struct {
        /* Stream 0 */
        volatile uint32_t FEIF0     : 1; // bit 0
        volatile uint32_t RESERVED0 : 1; // bit 1
        volatile uint32_t DMEIF0    : 1; // bit 2
        volatile uint32_t TEIF0     : 1; // bit 3
        volatile uint32_t HTIF0     : 1; // bit 4
        volatile uint32_t TCIF0     : 1; // bit 5
         volatile uint32_t FEIF1     : 1; // bit 8
        volatile uint32_t RESERVED1 : 1; // bit 6-7

        /* Stream 1 */


        volatile uint32_t DMEIF1    : 1; // bit 10
        volatile uint32_t TEIF1     : 1; // bit 11
        volatile uint32_t HTIF1     : 1; // bit 12
        volatile uint32_t TCIF1     : 1; // bit 13
        volatile uint32_t RESERVED3 : 4; // bit 14-15

        /* Stream 2 */
        volatile uint32_t FEIF2     : 1; // bit 16
        volatile uint32_t RESERVED4 : 1; // bit 17
        volatile uint32_t DMEIF2    : 1; // bit 18
        volatile uint32_t TEIF2     : 1; // bit 19
        volatile uint32_t HTIF2     : 1; // bit 20
        volatile uint32_t TCIF2     : 1; // bit 21
         volatile uint32_t FEIF3     : 1; // bit 24
        volatile uint32_t RESERVED5 : 1; // bit 22-23

        /* Stream 3 */

        //volatile uint32_t RESERVED6 : 1; // bit 25
        volatile uint32_t DMEIF3    : 1; // bit 26
        volatile uint32_t TEIF3     : 1; // bit 27
        volatile uint32_t HTIF3     : 1; // bit 28
        volatile uint32_t TCIF3     : 1; // bit 29
        volatile uint32_t RESERVED7 : 4; // bit 30-31
    } BITS;
} DMA_LISR_t;

/* D-01 WARNING: The BITS struct below does NOT accurately reflect the hardware register layout.
   The actual layout has non-uniform gaps between stream flag groups (RM0368 Table 42).
   Do NOT use BITS struct members directly — always use DMA_ReadISR() with the ALL
   register and the lookup table masks in DMA.c for correct flag access. */
typedef union {
    volatile uint32_t ALL;
    struct {
        /* Stream 4 */
        volatile uint32_t FEIF4     : 1; // bit 0
        volatile uint32_t RESERVED0 : 1; // bit 1
        volatile uint32_t DMEIF4    : 1; // bit 2
        volatile uint32_t TEIF4     : 1; // bit 3
        volatile uint32_t HTIF4     : 1; // bit 4
        volatile uint32_t TCIF4     : 1; // bit 5
        volatile uint32_t FEIF5     : 1; // bit 6
        volatile uint32_t RESERVED1 : 1; // bit 7

        /* Stream 5 */


        volatile uint32_t DMEIF5    : 1; // bit 8
        volatile uint32_t TEIF5     : 1; // bit 9
        volatile uint32_t HTIF5     : 1; // bit 10
        volatile uint32_t TCIF5     : 1; // bit 11
        volatile uint32_t RESERVED3 : 4; // bit 12

        /* Stream 6 */
        volatile uint32_t FEIF6     : 1; // bit 13
        volatile uint32_t RESERVED4 : 1; // bit 14
        volatile uint32_t DMEIF6    : 1; // bit 15
        volatile uint32_t TEIF6     : 1; // bit 16
        volatile uint32_t HTIF6     : 1; // bit 17
        volatile uint32_t TCIF6     : 1; // bit 18
        volatile uint32_t FEIF7     : 1; // bit 20
        volatile uint32_t RESERVED5 : 1; // bit 22-23

        /* Stream 7 */


        volatile uint32_t DMEIF7    : 1; // bit 26
        volatile uint32_t TEIF7     : 1; // bit 27
        volatile uint32_t HTIF7     : 1; // bit 28
        volatile uint32_t TCIF7     : 1; // bit 29
        volatile uint32_t RESERVED7 : 4; // bit 30-31
    } BITS;
} DMA_HISR_t;

/* D-01 WARNING: The BITS struct below does NOT accurately reflect the hardware register layout.
   The actual layout has non-uniform gaps between stream flag groups (RM0368 Table 42).
   Do NOT use BITS struct members directly — always use DMA_ReadISR() with the ALL
   register and the lookup table masks in DMA.c for correct flag access. */
typedef union {
    volatile uint32_t ALL;
    struct {
        /* Stream 0 */
        volatile uint32_t CFEIF0    : 1; // bit 0
        volatile uint32_t RESERVED0 : 1; // bit 1
        volatile uint32_t CDMEIF0   : 1; // bit 2
        volatile uint32_t CTEIF0    : 1; // bit 3
        volatile uint32_t CHTIF0    : 1; // bit 4
        volatile uint32_t CTCIF0    : 1; // bit 5
        volatile uint32_t CFEIF1    : 1; // bit 8
        volatile uint32_t RESERVED1 : 1; // bit 6-7

        /* Stream 1 */


        volatile uint32_t CDMEIF1   : 1; // bit 10
        volatile uint32_t CTEIF1    : 1; // bit 11
        volatile uint32_t CHTIF1    : 1; // bit 12
        volatile uint32_t CTCIF1    : 1; // bit 13
        volatile uint32_t RESERVED3 : 4; // bit 14-15

        /* Stream 2 */
        volatile uint32_t CFEIF2    : 1; // bit 16
        volatile uint32_t RESERVED4 : 1; // bit 17
        volatile uint32_t CDMEIF2   : 1; // bit 18
        volatile uint32_t CTEIF2    : 1; // bit 19
        volatile uint32_t CHTIF2    : 1; // bit 20
        volatile uint32_t CTCIF2    : 1; // bit 21
        volatile uint32_t CFEIF3    : 1; // bit 24
        volatile uint32_t RESERVED5 : 1; // bit 22-23

        /* Stream 3 */


        volatile uint32_t CDMEIF3   : 1; // bit 26
        volatile uint32_t CTEIF3    : 1; // bit 27
        volatile uint32_t CHTIF3    : 1; // bit 28
        volatile uint32_t CTCIF3    : 1; // bit 29
        volatile uint32_t RESERVED7 : 4; // bit 30-31
    } BITS;
} DMA_LIFCR_t;

/* D-01 WARNING: The BITS struct below does NOT accurately reflect the hardware register layout.
   The actual layout has non-uniform gaps between stream flag groups (RM0368 Table 42).
   Do NOT use BITS struct members directly — always use DMA_ReadISR() with the ALL
   register and the lookup table masks in DMA.c for correct flag access. */
typedef union {
    volatile uint32_t ALL;
    struct {
        /* Stream 4 */
        volatile uint32_t CFEIF4    : 1; // bit 0
        volatile uint32_t RESERVED0 : 1; // bit 1
        volatile uint32_t CDMEIF4   : 1; // bit 2
        volatile uint32_t CTEIF4    : 1; // bit 3
        volatile uint32_t CHTIF4    : 1; // bit 4
        volatile uint32_t CTCIF4    : 1; // bit 5
        volatile uint32_t CFEIF5    : 1; // bit 8
        volatile uint32_t RESERVED1 : 1; // bit 6-7

        /* Stream 5 */


        volatile uint32_t CDMEIF5   : 1; // bit 10
        volatile uint32_t CTEIF5    : 1; // bit 11
        volatile uint32_t CHTIF5    : 1; // bit 12
        volatile uint32_t CTCIF5    : 1; // bit 13
        volatile uint32_t RESERVED3 : 4; // bit 14-15

        /* Stream 6 */
        volatile uint32_t CFEIF6    : 1; // bit 16
        volatile uint32_t RESERVED4 : 1; // bit 17
        volatile uint32_t CDMEIF6   : 1; // bit 18
        volatile uint32_t CTEIF6    : 1; // bit 19
        volatile uint32_t CHTIF6    : 1; // bit 20
        volatile uint32_t CTCIF6    : 1; // bit 21
         volatile uint32_t CFEIF7    : 1; // bit 24
        volatile uint32_t RESERVED5 : 1; // bit 22-23

        /* Stream 7 */


        volatile uint32_t CDMEIF7   : 1; // bit 26
        volatile uint32_t CTEIF7    : 1; // bit 27
        volatile uint32_t CHTIF7    : 1; // bit 28
        volatile uint32_t CTCIF7    : 1; // bit 29
        volatile uint32_t RESERVED7 : 4; // bit 30-31
    } BITS;
} DMA_HIFCR_t;


/* D-05: volatile removed from all individual bit-field members below — volatile on individual
   bit-field members is non-standard per C99/C11. The volatile uint32_t ALL member of the
   enclosing union is sufficient to guarantee hardware-visible accesses. */
typedef union{
           volatile uint32_t ALL;

     struct{

        uint32_t EN :1;       /* D-05 */
        uint32_t DMEIE:1;     /* D-05 */
        uint32_t TEIE:1;      /* D-05 */
        uint32_t HTIE :1;     /* D-05 */
        uint32_t TCIE:1;      /* D-05 */
        uint32_t PFCTRL:1;    /* D-05 */
        uint32_t DIR:2;       /* D-05 */
        uint32_t CIRC:1;      /* D-05 */
        uint32_t PINC:1;      /* D-05 */
        uint32_t MINC:1;      /* D-05 */
        uint32_t PSIZE:2;     /* D-05 */
        uint32_t MSIZE:2;     /* D-05 */
        uint32_t PINCOS:1;    /* D-05 */
        uint32_t PL:2;        /* D-05 */
        uint32_t DBM:1;       /* D-05 */
        uint32_t CT:1;        /* D-05 */
        uint32_t RES0:1;      /* D-05 */
        uint32_t PBURST:2;    /* D-05 */
        uint32_t MBURST:2;    /* D-05 */
        uint32_t CHSEL:3;     /* D-05 */
        uint32_t RES1:4;      /* D-05 */



     }BITS;

}DMA_SxCR_t;



/* D-05: volatile removed from all individual bit-field members below — same reasoning as
   DMA_SxCR_t above. */
typedef union{
           volatile uint32_t ALL;

     struct{

        uint32_t FTH :2;   /* D-05 */
        uint32_t DMDIS:1;  /* D-05 */
        uint32_t FS:3;     /* D-05 */
        uint32_t RES0 :1;  /* D-05 */
        uint32_t FEIE:1;   /* D-05 */
        uint32_t RES1:24;  /* D-05 */




     }BITS;

}DMA_SxFCR_t;


typedef struct {
    DMA_SxCR_t        CR;              // 0x00
    volatile uint32_t NDTR;  /* D-03: volatile — decremented by hardware during transfer */
    volatile uint32_t PAR;   /* D-03: volatile — peripheral address register */
    volatile uint32_t M0AR;  /* D-03: volatile — memory 0 address register */
    volatile uint32_t M1AR;  /* D-03: volatile — memory 1 address register (double buffer) */
    DMA_SxFCR_t       FCR;             // 0x14
} DMA_Stream_t;

typedef struct {
    DMA_LISR_t   LISR;     // 0x00
    DMA_HISR_t   HISR;     // 0x04
    DMA_LIFCR_t  LIFCR;    // 0x08
    DMA_HIFCR_t  HIFCR;    // 0x0C
    DMA_Stream_t STREAM[8];
} DMA_REGS_t;


#define DMA1   ((volatile DMA_REGS_t *)DMA1_BASE_ADDR)  /* D-02: volatile — prevents compiler from caching hardware register reads */
#define DMA2   ((volatile DMA_REGS_t *)DMA2_BASE_ADDR)  /* D-02: volatile — prevents compiler from caching hardware register reads */


/*============================================================================
 *  FLAG BIT POSITION LOOKUP TABLE
 *============================================================================
 *
 *  STM32F401 LISR/HISR register layout (from RM0368):
 *
 *    Bits 31-28: Reserved
 *    Bits 27-22: Stream 3/7 flags [TCIF, HTIF, TEIF, DMEIF, reserved, FEIF]
 *    Bits 21-16: Stream 2/6 flags [TCIF, HTIF, TEIF, DMEIF, reserved, FEIF]
 *    Bits 15-12: Reserved
 *    Bits 11-6:  Stream 1/5 flags [TCIF, HTIF, TEIF, DMEIF, reserved, FEIF]
 *    Bits 5-0:   Stream 0/4 flags [TCIF, HTIF, TEIF, DMEIF, reserved, FEIF]
 *
 *  The layout is NOT uniform - there's a 4-bit gap between streams 1 and 2.
 *  Using a lookup table is the cleanest and most maintainable solution.
 *============================================================================*/

/* Bit offset within the 6-bit flag group for each stream */
#define DMA_FLAG_FEIF_OFFSET   0U
#define DMA_FLAG_DMEIF_OFFSET  2U
#define DMA_FLAG_TEIF_OFFSET   3U
#define DMA_FLAG_HTIF_OFFSET   4U
#define DMA_FLAG_TCIF_OFFSET   5U

/* All flags mask for one stream (relative to base): bits 0,2,3,4,5 = 0x3D */
#define DMA_ALL_FLAGS_MASK  0x3DU

/* D-04: All static const flag array definitions moved to DMA.c to prevent
   duplicate instantiation in every translation unit that includes this header. */


#endif  /* DMA_REGS_H_ */
