#ifndef RING_BUFFER_H
#define RING_BUFFER_H

/*****************************************************************************
 * RING_BUFFER.h — SPSC Lock-Free Ring Buffer for MPU6050_RawData_t samples
 *                  STM32F401CC bare-metal data collection project
 *
 * ═══════════════════════════════════════════════════════════════════════
 * SPSC INVARIANT — READ THIS BEFORE MODIFYING ANYTHING
 * ═══════════════════════════════════════════════════════════════════════
 *
 * This ring buffer is Single-Producer Single-Consumer (SPSC):
 *   Producer: DMA ISR callback — calls RingBuffer_Push() ONLY
 *   Consumer: main loop        — calls RingBuffer_Pop()  ONLY
 *
 * RB-C01: HEAD is written ONLY by the producer (ISR).
 *         TAIL is written ONLY by the consumer (main).
 *         If any other code writes head or tail, the lock-free design
 *         breaks silently — data corruption with no visible error.
 *
 * RB-C08: No interrupt disabling. No critical sections. The SPSC
 *         invariant plus DMB barriers are the synchronisation mechanism.
 *         Disabling IRQs in Push would add latency to the DMA callback.
 *
 *****************************************************************************/

#include "STD_TYPES.h"
#include "MPU6050.h"

/* ============================================================
 *  Capacity and Mask
 * ============================================================ */

/* ----------------------------------------------------------------
 * RING_BUFFER_CAPACITY — must be a power of 2 for masking to work.
 * 128 samples × 14 bytes = 1,792 bytes RAM.
 * At 100 Hz: holds 1.28 seconds of data before overflow.
 * Sized to absorb main-loop jitter (UART stalls, etc.) without
 * overflow, without wasting SRAM on the 256 KB STM32F401CC.
 * ---------------------------------------------------------------- */
#define RING_BUFFER_CAPACITY   128U

/* Derived mask — used for index wrapping without modulo.
 * Modulo on non-power-of-2 generates a division instruction on
 * Cortex-M4 (no hardware divide for arbitrary divisors). Masking
 * compiles to a single AND instruction.                          */
#define RING_BUFFER_MASK       (RING_BUFFER_CAPACITY - 1U)

/* ============================================================
 *  Error Codes
 * ============================================================ */

typedef enum
{
    RING_BUFFER_OK       = 0x00U,  /* Operation successful              */
    RING_BUFFER_FULL     = 0x01U,  /* Push rejected — buffer full       */
    RING_BUFFER_EMPTY    = 0x02U,  /* Pop rejected — buffer empty       */
    RING_BUFFER_NULL_PTR = 0x03U,   /* NULL sample pointer passed        */
    RING_BUFFER_PARAM    = 0x04U   /* Invalid parameter passed          */
} RingBuffer_Error_t;

/* ============================================================
 *  Initialization
 * ============================================================ */

/**
 * @brief  Initialize the ring buffer — clears head, tail, buf[].
 *         Must be called once before any Push or Pop.
 *         Safe to call again to reset the buffer mid-session.
 * @note   Not ISR-safe — call only from main context before
 *         enabling the TIM2 interrupt.
 * @return RING_BUFFER_OK always.
 */
RingBuffer_Error_t RingBuffer_Init(void);

/* ============================================================
 *  Producer API (ISR context)
 * ============================================================ */

/**
 * @brief  Push one MPU6050_RawData_t sample into the ring buffer.
 *
 *         ISR-safe. Lock-free SPSC design — no interrupt disable.
 *         Uses DMB barrier to guarantee write ordering on Cortex-M4.
 *
 *         If the buffer is full, the sample is DROPPED and the
 *         drop counter is incremented. Oldest data is NOT overwritten.
 *         Rationale: for road data collection, a dropped sample is
 *         better than corrupt data from overwrite during a read.
 *
 * @param  sample  Pointer to MPU6050_RawData_t to copy in
 *                 (must not be NULL)
 * @return RING_BUFFER_OK       on success
 *         RING_BUFFER_FULL     if no space — sample dropped
 *         RING_BUFFER_NULL_PTR if sample is NULL
 *
 * @note   SPSC invariant (RB-C01): ONLY this function writes head.
 *         Any other code writing head corrupts the buffer silently.
 */
RingBuffer_Error_t RingBuffer_Push(const MPU6050_RawData_t *sample);

/* ============================================================
 *  Consumer API (main loop context)
 * ============================================================ */

/**
 * @brief  Pop one MPU6050_RawData_t sample from the ring buffer.
 *
 *         Main-loop-safe. Lock-free SPSC design.
 *         Uses DMB barrier after read, before tail increment.
 *
 * @param  sample  Pointer to destination struct (must not be NULL)
 * @return RING_BUFFER_OK       on success
 *         RING_BUFFER_EMPTY    if no data available
 *         RING_BUFFER_NULL_PTR if sample is NULL
 *
 * @note   SPSC invariant (RB-C01): ONLY this function writes tail.
 *         Any other code writing tail corrupts the buffer silently.
 */
RingBuffer_Error_t RingBuffer_Pop(MPU6050_RawData_t *sample);

/* ============================================================
 *  Status Queries
 * ============================================================ */

/**
 * @brief  Return number of samples currently available to pop.
 *         Based on (head - tail) unsigned subtraction (RB-C04).
 *         Safe to call from either context.
 * @return sample count (0 = empty, RING_BUFFER_CAPACITY = full)
 */
uint32_t RingBuffer_Count(void);

/**
 * @brief  Return 1U if buffer is empty, 0U otherwise.
 *         Empty condition (RB-C03): head == tail.
 * @return 1U = empty, 0U = has data
 */
uint8_t RingBuffer_IsEmpty(void);

/**
 * @brief  Return 1U if buffer is full, 0U otherwise.
 *         Full condition (RB-C02): (head - tail) >= CAPACITY.
 * @return 1U = full, 0U = has space
 */
uint8_t RingBuffer_IsFull(void);

/* ============================================================
 *  Diagnostics
 * ============================================================ */

/**
 * @brief  Return total number of successful pushes since Init.
 *         Saturates at 0xFFFFFFFFUL.
 * @return push count
 */
uint32_t RingBuffer_GetPushCount(void);

/**
 * @brief  Return total number of successful pops since Init.
 *         Saturates at 0xFFFFFFFFUL.
 * @return pop count
 */
uint32_t RingBuffer_GetPopCount(void);

/**
 * @brief  Return total samples dropped (push when full) since Init.
 *         Saturates at 0xFFFFFFFFUL. Non-zero means main loop is
 *         too slow or TIM2 rate is too high.
 * @return drop count
 */
uint32_t RingBuffer_GetDropCount(void);


/* ============================================================
 *  RING_BUFFER.h — extended API for sliding-window consumers
 * ============================================================ */



/* NEW — for Thread 2's sliding window pattern */

/**
 * @brief Peek at N consecutive samples WITHOUT removing them.
 *        Copies samples[tail..tail+n-1] into dest. Tail is NOT advanced.
 *        Safe for SPSC: only reads, never modifies head/tail.
 *
 * @param dest   Destination array, must hold at least n samples
 * @param n      Number of samples to peek
 * @return RING_BUFFER_OK if n samples were available and copied
 *         RING_BUFFER_EMPTY if fewer than n samples available
 */
RingBuffer_Error_t RingBuffer_PeekWindow(MPU6050_RawData_t *dest, uint32_t n);

/**
 * @brief Advance the tail by N samples (effectively "consume" them).
 *        Used after PeekWindow to free up space for new samples.
 *
 * @param n  Number of samples to discard
 * @return RING_BUFFER_OK if n samples were available and discarded
 *         RING_BUFFER_EMPTY if fewer than n samples available
 */
RingBuffer_Error_t RingBuffer_Advance(uint32_t n);

#endif /* RING_BUFFER_H */