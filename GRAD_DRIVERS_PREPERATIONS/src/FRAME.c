/*****************************************************************************
 * FRAME.c — STM32F401CC bare-metal framing protocol
 *
 * NEW WIRE FORMAT (STM32 → ESP32, GPIO sync line used – no SYNC byte)
 *
 *   Offset      Field      Size       Notes
 *   ─────────────────────────────────────────────────────────────────────
 *   0           LEN        1 byte     bytes from TYPE to last CRC byte
 *                                     = payload_len + 6
 *                                     (TYPE + ECU + payload + CRC32)
 *   1           TYPE       1 byte     application-defined frame type
 *   2           ECU_ID     1 byte     source ECU identifier
 *   3..LEN-4    PAYLOAD    variable   application data
 *   LEN-3       CRC[31:24] 1 byte     CRC32 MSB (big-endian)
 *   LEN-2       CRC[23:16] 1 byte
 *   LEN-1       CRC[15:8]  1 byte
 *   LEN         CRC[7:0]   1 byte     CRC32 LSB
 *
 * CRC32 INPUT RANGE
 *   Computed over: TYPE + ECU_ID + PAYLOAD bytes
 *   Total input length = payload_len + 2 bytes
 *   Does NOT include LEN byte. Does NOT include the CRC itself.
 *
 * WHY NO SYNC BYTE IN OUTBOUND DIRECTION?
 *   An external GPIO line is toggled to mark frame start; this saves one
 *   byte per frame and avoids an additional state machine on the ESP32.
 *
 * WHY IS THE CRC OVER TYPE+ECU+PAYLOAD ONLY?
 *   The LEN byte is re-derivable and non-critical; including it would
 *   add no error detection for the data that matters. The teammate's
 *   ESP32 receiver uses this exact range.
 *
 * WHY 32 ITERATIONS PER BYTE (NOT 8)?
 *   The inner loop runs 32 iterations to match the teammate's ESP32
 *   calculateCRC32() reference exactly. This produces a result that is
 *   mathematically different from standard MPEG-2 CRC32 (it effectively
 *   processes each byte followed by 24 implicit zero bits). DO NOT CHANGE
 *   TO 8 ITERATIONS — both ends of the link must stay in sync.
 *
 *****************************************************************************/

#include "FRAME.h"

/* ------------------------------------------------------------------ */
/*  Static helper: big-endian pack of a 32-bit value into bytes       */
/* ------------------------------------------------------------------ */
static inline void frame_pack_u32_be(uint32_t val, uint8_t buf[4U])
{
    buf[0U] = (uint8_t)((val >> 24U) & 0xFFU);
    buf[1U] = (uint8_t)((val >> 16U) & 0xFFU);
    buf[2U] = (uint8_t)((val >>  8U) & 0xFFU);
    buf[3U] = (uint8_t)( val        & 0xFFU);
}

/* ------------------------------------------------------------------ */
/*  CRC32 – MPEG-2 style, 32-iteration inner loop (MATCHES EXISTING)  */
/* ------------------------------------------------------------------ */
uint32_t Frame_CRC32(const uint8_t *data, uint16_t len)
{
    uint32_t crc = FRAME_CRC32_INIT;                /* 0xFFFFFFFF */
    uint16_t i;

    if ((data == NULL) && (len > 0U))
    {
        /* Treat as empty input – preserve deterministic behaviour */
        return FRAME_CRC32_INIT;
    }

    for (i = 0U; i < len; i++)
    {
        uint8_t byte_in = data[i];
        uint32_t bit;

        /* XOR the next byte into the LSB position (ESP32 style) */
        crc ^= (uint32_t)byte_in;

        /* Process 32 bits (NOT 8) – intentional, must not be changed */
        for (bit = 0U; bit < 32U; bit++)
        {
            if ((crc & 0x80000000U) != 0U)
            {
                crc = (crc << 1U) ^ FRAME_CRC32_POLY;
            }
            else
            {
                crc = crc << 1U;
            }
        }
    }

    /* No final XOR to match ESP32 receiver */
    return crc;
}

/* ------------------------------------------------------------------ */
/*  Frame_Build – new wire format                                  */
/* ------------------------------------------------------------------ */
Frame_Status_t Frame_Build(uint8_t        type,
                           uint8_t        ecu_id,
                           const uint8_t *payload,
                           uint8_t        len,
                           uint8_t       *out_buf,
                           uint16_t       out_cap,
                           uint16_t      *out_len)
{
    uint16_t total_len;
    uint8_t  len_byte;
    uint32_t crc;
    uint16_t i;

    /* a) Null pointer checks */
    if ((out_buf == NULL) || (out_len == NULL))
    {
        return FRAME_ERR_NULL_PTR;
    }

    /* b) Payload pointer may be NULL only when len == 0 */
    if ((payload == NULL) && (len > 0U))
    {
        return FRAME_ERR_NULL_PTR;
    }

    /* c) Payload length bound */
    if (len > FRAME_MAX_PAYLOAD)
    {
        return FRAME_ERR_PAYLOAD_TOO_BIG;
    }

    /* d) Compute total frame size */
    total_len = (uint16_t)len + (uint16_t)FRAME_OVERHEAD_BYTES;  /* len + 7 */

    /* e) Output buffer capacity check */
    if (out_cap < total_len)
    {
        return FRAME_ERR_BUF_TOO_SMALL;
    }

    /* f) LEN byte = payload length + 6 (TYPE + ECU + payload + CRC) */
    len_byte = (uint8_t)(len + 6U);

    /* g) Write header */
    out_buf[0U] = len_byte;
    out_buf[1U] = type;
    out_buf[2U] = ecu_id;

    /* h) Copy payload */
    for (i = 0U; i < len; i++)
    {
        out_buf[3U + i] = payload[i];
    }

    /* i) Compute CRC over TYPE + ECU_ID + PAYLOAD (len+2 bytes) */
    crc = Frame_CRC32(&out_buf[1U], (uint16_t)(len + 2U));

    /* j) Append CRC in big-endian */
    frame_pack_u32_be(crc, &out_buf[3U + len]);

    /* k) Set output length */
    *out_len = total_len;

    return FRAME_OK;
}
