/*****************************************************************************
 * FRAME.c — Generic Framing Protocol with CRC32-MPEG2 Implementation
 *           STM32F401CC bare-metal data collection project
 *
 * Replaces the previous fixed-size 20-byte / CRC8 implementation.
 *
 * Design notes:
 *   - Stateless: no static counters, no static buffers, no module state.
 *     The caller owns the output buffer and decides framing cadence.
 *   - Bit-banged CRC32 with 32-iteration inner loop. This matches the
 *     teammate's reference exactly. NO LOOKUP TABLE — keeps flash usage
 *     down and avoids any risk of table/algorithm mismatch.
 *   - All loops are bounded by a uint16_t length and execute at most
 *     FRAME_MAX_PAYLOAD + 2 outer iterations × 32 inner = ~8000 ops,
 *     well within the 10 ms budget at 100 Hz on a 84 MHz Cortex-M4.
 *   - MISRA-C:2012 considerations:
 *       * No dynamic allocation.
 *       * No recursion.
 *       * Explicit unsigned suffixes on all integer literals.
 *       * Explicit casts on narrowing conversions.
 *       * Single return point per function would be possible but the
 *         current early-return validation pattern is clearer; the
 *         project style guide (see N-F06 in the previous module)
 *         already accepts this.
 *
 *****************************************************************************/

#include "FRAME.h"

/* ================================================================
 *  Internal Helpers
 * ================================================================ */

/**
 * @brief  Pack a uint32_t into a byte buffer in big-endian order.
 *
 *         Used to place the CRC32 trailer. MSB written first.
 *
 * @param[out] buf  Destination, must have at least 4 bytes available.
 * @param[in]  val  32-bit value to pack.
 */
static void frame_pack_u32_be(uint8_t *buf, uint32_t val)
{
    buf[0U] = (uint8_t)((val >> 24U) & 0xFFU);
    buf[1U] = (uint8_t)((val >> 16U) & 0xFFU);
    buf[2U] = (uint8_t)((val >>  8U) & 0xFFU);
    buf[3U] = (uint8_t)( val         & 0xFFU);
}

/* ================================================================
 *  CRC32 Computation — MPEG-2 polynomial, 32-iteration inner loop
 *
 *  Algorithm (matches teammate's calculateCRC32 exactly):
 *
 *      crc = 0xFFFFFFFF
 *      for each byte b in data:
 *          crc ^= ((uint32_t)b << 24)
 *          for inner = 0..31:                  // 32 iterations, NOT 8
 *              if (crc & 0x80000000):
 *                  crc = (crc << 1) ^ 0x04C11DB7
 *              else:
 *                  crc = (crc << 1)
 *      return crc ^ 0xFFFFFFFF
 *
 *  ⚠️  The 32-iteration inner loop is intentional and unusual.
 *      Standard MPEG-2 CRC32 uses 8 iterations. With 32 iterations,
 *      each byte is processed and then 24 zero bits are shifted
 *      through the register, producing a different (but deterministic)
 *      result. Both endpoints of the link must use this same variant.
 * ================================================================ */

uint32_t Frame_CRC32(const uint8_t *data, uint16_t len)
{
    uint32_t crc = FRAME_CRC32_INIT;
    uint16_t i;
    uint8_t  inner;

    /* NULL-with-zero-length is a valid no-op; NULL-with-positive-length
     * is a programming error — return INIT^XOROUT == 0x00000000 to
     * signal an obviously-wrong CRC to any sane receiver. */
    if ((data == NULL) && (len > 0U))
    {
        return FRAME_CRC32_INIT ^ FRAME_CRC32_XOROUT;
    }

    for (i = 0U; i < len; i++)
    {
        crc ^= ((uint32_t)data[i] << 24U);

        for (inner = 0U; inner < 32U; inner++)
        {
            if ((crc & 0x80000000U) != 0U)
            {
                crc = (crc << 1U) ^ FRAME_CRC32_POLY;
            }
            else
            {
                crc = (crc << 1U);
            }
        }
    }

    return crc ^ FRAME_CRC32_XOROUT;
}

/* ================================================================
 *  Frame Build
 * ================================================================ */

Frame_Status_t Frame_Build(uint8_t        type,
                           const uint8_t *payload,
                           uint8_t        len,
                           uint8_t       *out_buf,
                           uint16_t       out_cap,
                           uint16_t      *out_len)
{
    uint16_t total_len;
    uint16_t i;
    uint32_t crc;

    /* -------------------------------------------------------
     * 1) Validate output pointers
     * ------------------------------------------------------- */
    if (out_buf == NULL)
    {
        return FRAME_ERR_NULL_PTR;
    }
    if (out_len == NULL)
    {
        return FRAME_ERR_NULL_PTR;
    }

    /* -------------------------------------------------------
     * 2) Validate payload pointer / length consistency
     *    NULL payload is allowed only when len == 0
     *    (e.g. FRAME_TYPE_BL_ENTER has no payload).
     * ------------------------------------------------------- */
    if ((payload == NULL) && (len > 0U))
    {
        return FRAME_ERR_NULL_PTR;
    }

    /* -------------------------------------------------------
     * 3) Validate payload size against protocol maximum
     * ------------------------------------------------------- */
    if (len > FRAME_MAX_PAYLOAD)
    {
        return FRAME_ERR_PAYLOAD_TOO_BIG;
    }

    /* -------------------------------------------------------
     * 4) Validate output buffer capacity
     *    Total frame size = HEADER (3) + payload + CRC (4)
     * ------------------------------------------------------- */
    total_len = (uint16_t)len + (uint16_t)FRAME_OVERHEAD_BYTES;

    if (out_cap < total_len)
    {
        return FRAME_ERR_BUF_TOO_SMALL;
    }

    /* -------------------------------------------------------
     * 5) Write header: SYNC | TYPE | LEN
     * ------------------------------------------------------- */
    out_buf[0U] = FRAME_SYNC_BYTE;
    out_buf[1U] = type;
    out_buf[2U] = len;

    /* -------------------------------------------------------
     * 6) Copy payload bytes (if any)
     *    Bounded by uint8_t len <= 248, no overrun possible
     *    after the capacity check above.
     * ------------------------------------------------------- */
    for (i = 0U; i < (uint16_t)len; i++)
    {
        out_buf[FRAME_HEADER_BYTES + i] = payload[i];
    }

    /* -------------------------------------------------------
     * 7) Compute CRC32 over TYPE + LEN + PAYLOAD
     *    That is, bytes [1..2+len-1] of out_buf, total len+2.
     *    We pass &out_buf[1] so the SYNC byte is excluded.
     * ------------------------------------------------------- */
    crc = Frame_CRC32(&out_buf[1U], (uint16_t)((uint16_t)len + 2U));

    /* -------------------------------------------------------
     * 8) Append CRC32 in big-endian order at offset LEN+3
     * ------------------------------------------------------- */
    frame_pack_u32_be(&out_buf[FRAME_HEADER_BYTES + (uint16_t)len], crc);

    /* -------------------------------------------------------
     * 9) Report total bytes written
     * ------------------------------------------------------- */
    *out_len = total_len;

    return FRAME_OK;
}
