/*****************************************************************************
 * RING_BUFFER.c — SPSC Lock-Free Ring Buffer Implementation
 *                  STM32F401CC bare-metal data collection project
 *
 * ═══════════════════════════════════════════════════════════════════════
 * SPSC INVARIANT — CRITICAL
 * ═══════════════════════════════════════════════════════════════════════
 *
 * RB-C01: HEAD written ONLY by producer (RingBuffer_Push / DMA ISR).
 *         TAIL written ONLY by consumer (RingBuffer_Pop  / main loop).
 *         No other code may write head or tail.
 *
 * RB-C07: DMB instruction on Cortex-M4:
 *         __asm volatile ("dmb" ::: "memory");
 *         The "memory" clobber prevents the COMPILER from reordering.
 *         The "dmb" instruction prevents the CPU from reordering.
 *         Both are needed — compiler barrier alone is insufficient.
 *         DMB (Data Memory Barrier) is correct here — not DSB (stronger,
 *         slower, unnecessary for SPSC data ordering).
 *
 * RB-C08: No interrupt disabling anywhere in this module.
 *
 *****************************************************************************/

#include "RING_BUFFER.h"

/* ================================================================
 *  Internal Storage Struct — NOT exposed in header
 * ================================================================ */

/* RingBuffer_t — single global instance for this bare-metal project.
 *
 * volatile on head and tail:
 *   head is written by ISR (producer), read by main (consumer).
 *   tail is written by main (consumer), read by ISR (producer).
 *   Both MUST be volatile to prevent the compiler from caching
 *   their values in a register across loop iterations.
 *
 * buf[] is NOT volatile:
 *   Data is accessed atomically per element via explicit field copy.
 *   The volatile head/tail indices are the synchronisation mechanism,
 *   not volatile on the data array itself.                           */
typedef struct
{
    MPU6050_RawData_t   buf[RING_BUFFER_CAPACITY];
    volatile uint32_t   head;   /* write index — producer only */
    volatile uint32_t   tail;   /* read  index — consumer only */
} RingBuffer_t;

/* ================================================================
 *  Static State — all file-scope, no dynamic allocation
 * ================================================================ */

/* Single global ring buffer instance.
 * Rationale: bare-metal single-sensor design — one ring buffer.
 * If a second is ever needed, add a second module.                */
static RingBuffer_t ring_buffer_instance;

/* ================================================================
 *  Debug Breadcrumbs — readable in Watch panel
 * ================================================================ */

/* volatile: may be inspected by debugger asynchronously.
 *
 * dbg_rb_stage values:
 *   0x00 = uninitialised
 *   0x01 = RingBuffer_Init entered
 *   0x63 = RingBuffer_Init SUCCESS (99 decimal)
 *   0xA0 = Push entered
 *   0xA1 = Push: buffer full — sample dropped
 *   0xA2 = Push: write complete, head incremented
 *   0xB0 = Pop entered
 *   0xB1 = Pop: buffer empty
 *   0xB2 = Pop: read complete, tail incremented                  */
static volatile uint8_t dbg_rb_stage = 0U;

/* ================================================================
 *  Lifetime Statistics — never cleared, saturate at max
 * ================================================================ */

/* rb_push_count: written only by producer (ISR) — no race.
 * rb_pop_count:  written only by consumer (main) — no race.
 * rb_drop_count: written only by producer (ISR) — no race.
 * Not volatile: each is single-writer. Read from the other context
 * may see a stale value by one count — acceptable for diagnostics. */
static uint32_t rb_push_count = 0U;
static uint32_t rb_pop_count  = 0U;
static uint32_t rb_drop_count = 0U;

/* ================================================================
 *  RingBuffer_Init
 * ================================================================ */

/**
 * @brief  Initialize the ring buffer — clears head, tail, buf[],
 *         and all diagnostic counters.
 * @return RING_BUFFER_OK always.
 */
RingBuffer_Error_t RingBuffer_Init(void)
{
    uint32_t i;

    /* ---- 1) Debug breadcrumb: Init entered ---- */
    dbg_rb_stage = 0x01U;

    /* ---- 2) Reset head and tail indices ---- */
    ring_buffer_instance.head = 0U;
    ring_buffer_instance.tail = 0U;

    /* ---- 3) Zero all fields of all 128 buf[] entries ----
     *   No memset — explicit field-by-field loop.
     *   memset treats the struct as a raw byte array and is
     *   not MISRA-clean for structured types.                */
    for (i = 0U; i < RING_BUFFER_CAPACITY; i++)
    {
        ring_buffer_instance.buf[i].accel_x  = 0;
        ring_buffer_instance.buf[i].accel_y  = 0;
        ring_buffer_instance.buf[i].accel_z  = 0;
        ring_buffer_instance.buf[i].temp_raw = 0;
        ring_buffer_instance.buf[i].gyro_x   = 0;
        ring_buffer_instance.buf[i].gyro_y   = 0;
        ring_buffer_instance.buf[i].gyro_z   = 0;
    }

    /* ---- 4) Reset diagnostic counters ---- */
    rb_push_count = 0U;
    rb_pop_count  = 0U;
    rb_drop_count = 0U;

    /* ---- 5) Debug breadcrumb: Init SUCCESS ---- */
    dbg_rb_stage = 0x63U;   /* 99 decimal */

    /* ---- 6) Return ---- */
    return RING_BUFFER_OK;
}

/* ================================================================
 *  RingBuffer_Push  (PRODUCER — ISR context)
 *
 *  RB-C05 sequence:
 *    1. NULL check
 *    2. Read local copies of head and tail
 *    3. Full check via subtraction (RB-C02)
 *    4. Field-by-field copy into buf[head & MASK]
 *    5. DMB barrier (RB-C07) — guarantees data visible before head++
 *    6. Increment head — makes slot visible to consumer
 *    7. Increment push count (saturating)
 * ================================================================ */

RingBuffer_Error_t RingBuffer_Push(const MPU6050_RawData_t *sample)
{
    uint32_t local_head;
    uint32_t local_tail;
    uint32_t idx;

    dbg_rb_stage = 0xA0U;

    /* ---- 1) NULL check ---- */
    if (sample == NULL)
    {
        return RING_BUFFER_NULL_PTR;
    }

    /* ---- 2) Read local copies of head and tail ----
     *   local_head: owned by us (producer), reading our own index.
     *   local_tail: owned by consumer — volatile read gets latest value. */
    local_head = ring_buffer_instance.head;
    local_tail = ring_buffer_instance.tail;

    /* ---- 3) Full check (RB-C02) ----
     *   Subtraction-based: works correctly even when head wraps
     *   past UINT32_MAX because unsigned arithmetic wraps identically.
     *   Example: head=0, tail=UINT32_MAX → head-tail = 1 (not full). */
    if ((local_head - local_tail) >= RING_BUFFER_CAPACITY)
    {
        /* Buffer full — DROP the sample, do not overwrite oldest.
         * Rationale: for road data collection, a dropped sample is
         * better than corrupt data from overwrite during a read.   */
        if (rb_drop_count < 0xFFFFFFFFUL)
        {
            rb_drop_count++;
        }
        dbg_rb_stage = 0xA1U;
        return RING_BUFFER_FULL;
    }

    /* ---- 4) Copy sample into buf[head & MASK] ----
     *   Explicit field-by-field — no memcpy, no struct assignment
     *   through volatile pointer (undefined in C99).               */
    idx = local_head & RING_BUFFER_MASK;

    ring_buffer_instance.buf[idx].accel_x  = sample->accel_x;
    ring_buffer_instance.buf[idx].accel_y  = sample->accel_y;
    ring_buffer_instance.buf[idx].accel_z  = sample->accel_z;
    ring_buffer_instance.buf[idx].temp_raw = sample->temp_raw;
    ring_buffer_instance.buf[idx].gyro_x   = sample->gyro_x;
    ring_buffer_instance.buf[idx].gyro_y   = sample->gyro_y;
    ring_buffer_instance.buf[idx].gyro_z   = sample->gyro_z;

    /* ---- 5) DMB barrier (RB-C05, RB-C07) ----
     *   Ensures all buf[] writes are globally visible BEFORE
     *   head is incremented. Without this, the compiler or CPU
     *   may reorder the head increment before the data write.
     *   The consumer reads head to decide if data is available —
     *   if head is incremented before data is written, consumer
     *   reads garbage.
     *
     *   "memory" clobber = compiler barrier (prevents reordering)
     *   "dmb"           = CPU barrier (prevents store reordering) */
    __asm volatile ("dmb" ::: "memory");

    /* ---- 6) Increment head — makes slot visible to consumer ---- */
    ring_buffer_instance.head = local_head + 1U;

    /* ---- 7) Increment push count (saturating) ---- */
    if (rb_push_count < 0xFFFFFFFFUL)
    {
        rb_push_count++;
    }

    dbg_rb_stage = 0xA2U;

    /* ---- 8) Return success ---- */
    return RING_BUFFER_OK;
}

/* ================================================================
 *  RingBuffer_Pop  (CONSUMER — main loop context)
 *
 *  RB-C06 sequence:
 *    1. NULL check
 *    2. Read local copies of head and tail
 *    3. Empty check (RB-C03)
 *    4. Field-by-field copy from buf[tail & MASK] into *sample
 *    5. DMB barrier (RB-C07) — guarantees read completes before tail++
 *    6. Increment tail — frees the slot for the producer
 *    7. Increment pop count (saturating)
 * ================================================================ */

RingBuffer_Error_t RingBuffer_Pop(MPU6050_RawData_t *sample)
{
    uint32_t local_head;
    uint32_t local_tail;
    uint32_t idx;

    dbg_rb_stage = 0xB0U;

    /* ---- 1) NULL check ---- */
    if (sample == NULL)
    {
        return RING_BUFFER_NULL_PTR;
    }

    /* ---- 2) Read local copies of head and tail ----
     *   local_tail: owned by us (consumer), reading our own index.
     *   local_head: owned by producer — volatile read gets latest value. */
    local_head = ring_buffer_instance.head;
    local_tail = ring_buffer_instance.tail;

    /* ---- 3) Empty check (RB-C03) ---- */
    if (local_head == local_tail)
    {
        dbg_rb_stage = 0xB1U;
        return RING_BUFFER_EMPTY;
    }

    /* ---- 4) Copy fields from buf[tail & MASK] into *sample ----
     *   Explicit field-by-field — no memcpy.                       */
    idx = local_tail & RING_BUFFER_MASK;

    sample->accel_x  = ring_buffer_instance.buf[idx].accel_x;
    sample->accel_y  = ring_buffer_instance.buf[idx].accel_y;
    sample->accel_z  = ring_buffer_instance.buf[idx].accel_z;
    sample->temp_raw = ring_buffer_instance.buf[idx].temp_raw;
    sample->gyro_x   = ring_buffer_instance.buf[idx].gyro_x;
    sample->gyro_y   = ring_buffer_instance.buf[idx].gyro_y;
    sample->gyro_z   = ring_buffer_instance.buf[idx].gyro_z;

    /* ---- 5) DMB barrier (RB-C06, RB-C07) ----
     *   Ensures all buf[] reads complete BEFORE tail is incremented.
     *   Without this, tail increment may be reordered before the
     *   buf[] read. If the producer sees the freed slot and
     *   immediately overwrites it before consumer finishes reading,
     *   the consumer gets torn data.
     *
     *   "memory" clobber = compiler barrier
     *   "dmb"           = CPU barrier                               */
    __asm volatile ("dmb" ::: "memory");

    /* ---- 6) Increment tail — frees the slot for the producer ---- */
    ring_buffer_instance.tail = local_tail + 1U;

    /* ---- 7) Increment pop count (saturating) ---- */
    if (rb_pop_count < 0xFFFFFFFFUL)
    {
        rb_pop_count++;
    }

    dbg_rb_stage = 0xB2U;

    /* ---- 8) Return success ---- */
    return RING_BUFFER_OK;
}

/* ================================================================
 *  Status Queries
 * ================================================================ */

/**
 * @brief  Return number of samples currently available to pop.
 *         Unsigned subtraction (RB-C04) — wraps correctly at UINT32_MAX.
 * @return sample count (0 = empty, RING_BUFFER_CAPACITY = full)
 */
uint32_t RingBuffer_Count(void)
{
    return (ring_buffer_instance.head - ring_buffer_instance.tail);
}

/**
 * @brief  Return 1U if buffer is empty (RB-C03), 0U otherwise.
 * @return 1U = empty, 0U = has data
 */
uint8_t RingBuffer_IsEmpty(void)
{
    uint8_t result;

    if (ring_buffer_instance.head == ring_buffer_instance.tail)
    {
        result = 1U;
    }
    else
    {
        result = 0U;
    }

    return result;
}

/**
 * @brief  Return 1U if buffer is full (RB-C02), 0U otherwise.
 * @return 1U = full, 0U = has space
 */
uint8_t RingBuffer_IsFull(void)
{
    uint8_t result;

    if ((ring_buffer_instance.head - ring_buffer_instance.tail) >= RING_BUFFER_CAPACITY)
    {
        result = 1U;
    }
    else
    {
        result = 0U;
    }

    return result;
}

/* ================================================================
 *  Diagnostics
 * ================================================================ */

/**
 * @brief  Return total successful pushes since Init.
 *         Saturates at 0xFFFFFFFFUL.
 * @return push count
 */
uint32_t RingBuffer_GetPushCount(void)
{
    return rb_push_count;
}

/**
 * @brief  Return total successful pops since Init.
 *         Saturates at 0xFFFFFFFFUL.
 * @return pop count
 */
uint32_t RingBuffer_GetPopCount(void)
{
    return rb_pop_count;
}

/**
 * @brief  Return total samples dropped (push when full) since Init.
 *         Non-zero means main loop is too slow or TIM2 rate is too high.
 * @return drop count
 */
uint32_t RingBuffer_GetDropCount(void)
{
    return rb_drop_count;
}


/* ================================================================
 *  RingBuffer_PeekWindow  (CONSUMER — main loop context)
 *
 *  RB-C09 sequence:
 *    1. NULL check on dest
 *    2. Range check on n
 *    3. Read local copies of head and tail
 *    4. Availability check via subtraction (RB-C04)
 *    5. Field-by-field copy of n samples starting at tail
 *       (tail is NOT advanced — that's Advance's job)
 *    6. NO DMB needed: we only READ buf[]; tail is unchanged.
 *
 *  SPSC SAFETY:
 *    - Producer (Push) only writes head and slots beyond head
 *    - We read slots in [tail, tail+n-1] which lie BEHIND head
 *    - Therefore producer cannot overwrite slots we're reading
 *      as long as Count() < CAPACITY (no wraparound interference)
 * ================================================================ */

RingBuffer_Error_t RingBuffer_PeekWindow(MPU6050_RawData_t *dest, uint32_t n)
{
    uint32_t local_head;
    uint32_t local_tail;
    uint32_t idx;
    uint32_t i;

    dbg_rb_stage = 0xC0U;

    /* ---- 1) NULL check ---- */
    if (dest == NULL)
    {
        return RING_BUFFER_NULL_PTR;
    }

    /* ---- 2) Range check on n ----
     *   n == 0 makes no sense; n > CAPACITY can never be satisfied. */
    if ((n == 0U) || (n > RING_BUFFER_CAPACITY))
    {
        dbg_rb_stage = 0xC1U;
        return RING_BUFFER_PARAM;
    }

    /* ---- 3) Read local copies of head and tail ----
     *   local_tail: owned by us (consumer), reading our own index.
     *   local_head: owned by producer — volatile read gets latest value. */
    local_head = ring_buffer_instance.head;
    local_tail = ring_buffer_instance.tail;

    /* ---- 4) Availability check (RB-C04) ----
     *   Need at least n samples in [tail, head). */
    if ((local_head - local_tail) < n)
    {
        dbg_rb_stage = 0xC2U;
        return RING_BUFFER_EMPTY;
    }

    /* ---- 5) Field-by-field copy of n samples ----
     *   Walking from tail forward — each index masked into buf[].
     *   Tail itself is NOT incremented; multiple Peeks return the
     *   same data until Advance is called.                          */
    for (i = 0U; i < n; i++)
    {
        idx = (local_tail + i) & RING_BUFFER_MASK;

        dest[i].accel_x  = ring_buffer_instance.buf[idx].accel_x;
        dest[i].accel_y  = ring_buffer_instance.buf[idx].accel_y;
        dest[i].accel_z  = ring_buffer_instance.buf[idx].accel_z;
        dest[i].temp_raw = ring_buffer_instance.buf[idx].temp_raw;
        dest[i].gyro_x   = ring_buffer_instance.buf[idx].gyro_x;
        dest[i].gyro_y   = ring_buffer_instance.buf[idx].gyro_y;
        dest[i].gyro_z   = ring_buffer_instance.buf[idx].gyro_z;
    }

    /* ---- 6) No DMB and no tail update ----
     *   PeekWindow is a pure read. There is no publishing event,
     *   so no memory barrier is required. Pop and Advance are the
     *   only functions that publish via tail.                       */

    dbg_rb_stage = 0xC3U;

    /* ---- 7) Return success ---- */
    return RING_BUFFER_OK;
}

/* ================================================================
 *  RingBuffer_Advance  (CONSUMER — main loop context)
 *
 *  RB-C10 sequence:
 *    1. Range check on n
 *    2. Read local copies of head and tail
 *    3. Availability check via subtraction (RB-C04)
 *    4. DMB barrier — guarantees any prior PeekWindow reads complete
 *       BEFORE tail advances (otherwise producer could overwrite
 *       slots while consumer is still reading them).
 *    5. Advance tail by n
 *    6. Increment pop count by n (saturating)
 *
 *  CALLER CONTRACT:
 *    Advance is only valid after a successful PeekWindow that
 *    returned at least n samples. Calling Advance without a prior
 *    Peek is allowed but discards data without inspecting it.
 * ================================================================ */

RingBuffer_Error_t RingBuffer_Advance(uint32_t n)
{
    uint32_t local_head;
    uint32_t local_tail;

    dbg_rb_stage = 0xD0U;

    /* ---- 1) Range check on n ---- */
    if ((n == 0U) || (n > RING_BUFFER_CAPACITY))
    {
        dbg_rb_stage = 0xD1U;
        return RING_BUFFER_PARAM;
    }

    /* ---- 2) Read local copies of head and tail ---- */
    local_head = ring_buffer_instance.head;
    local_tail = ring_buffer_instance.tail;

    /* ---- 3) Availability check (RB-C04) ----
     *   Cannot advance past head. */
    if ((local_head - local_tail) < n)
    {
        dbg_rb_stage = 0xD2U;
        return RING_BUFFER_EMPTY;
    }

    /* ---- 4) DMB barrier (RB-C07) ----
     *   Ensures all prior buf[] reads (from PeekWindow) globally
     *   complete BEFORE tail is published. Otherwise the producer
     *   could see freed slots and overwrite them while a slow
     *   consumer is still finalizing reads from those positions. */
    __asm volatile ("dmb" ::: "memory");

    /* ---- 5) Advance tail by n —
     *   frees n slots for the producer. */
    ring_buffer_instance.tail = local_tail + n;

    /* ---- 6) Increment pop count by n (saturating) ----
     *   Each advanced sample counts as a "pop" for diagnostics.
     *   Saturate carefully to avoid overflow. */
    if ((rb_pop_count + n) >= rb_pop_count)   /* no wrap */
    {
        rb_pop_count += n;
    }
    else
    {
        rb_pop_count = 0xFFFFFFFFUL;
    }

    dbg_rb_stage = 0xD3U;

    /* ---- 7) Return success ---- */
    return RING_BUFFER_OK;
}