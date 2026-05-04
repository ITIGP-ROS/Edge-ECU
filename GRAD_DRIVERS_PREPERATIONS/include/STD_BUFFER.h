#ifndef STD_BUFFER_H
#define STD_BUFFER_H

#include "STD_TYPES.h"

/* ================================================================
 *  Buffer_t — typed buffer descriptor
 *
 *  Replaces the raw (uint8_t *buf, uint16_t len) pattern used
 *  across all drivers. Carries data pointer, capacity, valid
 *  length, and current read/write position in one struct.
 *
 *  Usage:
 *    Static allocation:
 *      static uint8_t raw[64];
 *      Buffer_t buf = BUFFER_INIT(raw, 64);
 *
 *    Fill then send:
 *      buf.data[0] = 0xAA;
 *      buf.data[1] = 0x55;
 *      buf.length  = 2U;
 *      UART_SVC_Transmit(UART1_ID, &buf);
 *
 *    Receive into buffer:
 *      BUFFER_RESET(&buf);
 *      UART_SVC_Receive(UART1_ID, &buf);
 *      // buf.length now holds bytes received
 * ================================================================ */

typedef struct
{
    uint8_t  *data;    /* pointer to raw byte array — must not be NULL */
    uint16_t  size;    /* total allocated capacity of data[] in bytes  */
    uint16_t  length;  /* number of valid/written bytes in data[]       */
    uint16_t  index;   /* current read or write cursor position         */
} Buffer_t;

/* ================================================================
 *  Static initialiser macro
 *  Usage: Buffer_t b = BUFFER_INIT(array, sizeof(array));
 * ================================================================ */
#define BUFFER_INIT(arr, cap) \
    { (arr), (uint16_t)(cap), 0U, 0U }

/* ================================================================
 *  Predicate macros — evaluate to 1U (true) or 0U (false)
 * ================================================================ */

/* True if length == 0 (no valid bytes) */
#define BUFFER_IS_EMPTY(b)   ((b)->length == 0U)

/* True if length == size (no space left to write) */
#define BUFFER_IS_FULL(b)    ((b)->length >= (b)->size)

/* True if index has reached the end of valid data */
#define BUFFER_READ_DONE(b)  ((b)->index >= (b)->length)

/* ================================================================
 *  Size query macros
 * ================================================================ */

/* Bytes available to read from current index */
#define BUFFER_REMAINING(b)  ((uint16_t)((b)->length - (b)->index))

/* Bytes of free space left to write */
#define BUFFER_FREE(b)       ((uint16_t)((b)->size - (b)->length))

/* ================================================================
 *  Mutation macros
 * ================================================================ */

/* Reset index and length — buffer is logically empty, data unchanged */
#define BUFFER_RESET(b) \
    do { (b)->index = 0U; (b)->length = 0U; } while (0)

/* Reset only the read cursor — re-read same data from start */
#define BUFFER_REWIND(b) \
    do { (b)->index = 0U; } while (0)

/* Advance index by n bytes — use after manually consuming bytes */
#define BUFFER_ADVANCE(b, n) \
    do { (b)->index = (uint16_t)((b)->index + (uint16_t)(n)); } while (0)

/* ================================================================
 *  Validation macro — checks all fields are self-consistent
 *  Returns 1U if valid, 0U if not
 *  Use at function entry when accepting a Buffer_t from a caller
 * ================================================================ */
#define BUFFER_IS_VALID(b) \
    (((b) != NULL)              && \
     ((b)->data   != NULL)      && \
     ((b)->size   >  0U)        && \
     ((b)->length <= (b)->size) && \
     ((b)->index  <= (b)->length))

/* ================================================================
 *  Const variant — for read-only (TX) buffers
 *  Drivers that only read from the buffer take const Buffer_t *
 *  Drivers that write into the buffer take Buffer_t *
 * ================================================================ */
#define CONST_BUFFER_IS_VALID(b) \
    (((b) != NULL)              && \
     ((b)->data   != NULL)      && \
     ((b)->size   >  0U)        && \
     ((b)->length <= (b)->size) && \
     ((b)->index  <= (b)->length))

#endif /* STD_BUFFER_H */
