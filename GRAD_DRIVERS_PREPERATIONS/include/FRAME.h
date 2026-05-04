/*****************************************************************************
 * FRAME.h — Generic Framing Protocol with CRC32-MPEG2 Public API
 *           STM32F401CC bare-metal data collection project
 *
 * Builds variable-length frames for UART transport between the STM32 and
 * a host (laptop / ESP32 bridge). Frame format is:
 *
 *   [SYNC=0xAA][TYPE][LEN][payload 0..LEN-1][CRC32 big-endian, 4 bytes]
 *
 * The CRC32 is computed over TYPE + LEN + PAYLOAD (LEN+2 bytes total).
 * It does NOT include the SYNC byte and does NOT include itself.
 *
 * ═══════════════════════════════════════════════════════════════════════
 * WIRE FORMAT — variable length, max 255 bytes total
 * ═══════════════════════════════════════════════════════════════════════
 *
 *   Offset      Field     Size       Description
 *   ─────────────────────────────────────────────────────────────────────
 *   0           SYNC      1 byte     Always 0xAAU — frame start marker
 *   1           TYPE      1 byte     Application-defined frame type code
 *   2           LEN       1 byte     Payload length, 0..248
 *   3..LEN+2    PAYLOAD   LEN bytes  Application data
 *   LEN+3       CRC[31:24]  1 byte   CRC32 MSB
 *   LEN+4       CRC[23:16]  1 byte
 *   LEN+5       CRC[15:8]   1 byte
 *   LEN+6       CRC[7:0]    1 byte   CRC32 LSB
 *
 * CRC32 INPUT
 *   The CRC32 is computed over bytes [TYPE, LEN, payload[0..LEN-1]]
 *   — that is, LEN+2 bytes. SYNC and the CRC field itself are excluded.
 *
 * CRC32 ALGORITHM (MPEG-2 style, non-reflected)
 *   Polynomial : 0x04C11DB7
 *   Initial    : 0xFFFFFFFF
 *   Final XOR  : 0xFFFFFFFF
 *   Reflect in : NO
 *   Reflect out: NO
 *
 *   ⚠️  IMPORTANT: This implementation uses an INNER LOOP OF 32 ITERATIONS
 *       per byte, not the standard 8. This matches the teammate's
 *       calculateCRC32() reference exactly. The result is mathematically
 *       different from a standard MPEG-2 CRC32 — it is effectively
 *       "byte XOR-then-shift-32-bits", which means each input byte is
 *       followed by 24 implicit zero bits during processing.
 *
 *       DO NOT "fix" this to 8 iterations. Both ends of the link must
 *       agree, and the teammate's ESP32 firmware is the authoritative
 *       reference until coordinated otherwise.
 *
 *****************************************************************************/

#ifndef FRAME_H
#define FRAME_H

#include "STD_TYPES.h"

/* ================================================================
 *  Frame Type Codes (application-defined)
 * ================================================================ */

#define FRAME_TYPE_CLASSIFICATION   0x01U   /* CNN classification result    */
#define FRAME_TYPE_RAW_IMU          0x02U   /* Raw MPU6050 sample           */
#define FRAME_TYPE_BL_DATA          0xFDU   /* Bootloader data chunk        */
#define FRAME_TYPE_BL_ENTER         0xFEU   /* Enter bootloader request     */
#define FRAME_TYPE_BL_ACK           0xFFU   /* Bootloader acknowledgement   */

/* ================================================================
 *  Frame Layout Constants
 * ================================================================ */

#define FRAME_SYNC_BYTE             0xAAU   /* Start-of-frame marker        */
#define FRAME_HEADER_BYTES          3U      /* SYNC + TYPE + LEN            */
#define FRAME_CRC_BYTES             4U      /* CRC32 trailer                */
#define FRAME_OVERHEAD_BYTES        7U      /* HEADER (3) + CRC (4)         */
#define FRAME_MAX_PAYLOAD           248U    /* Keeps total frame <= 255     */
#define FRAME_MAX_TOTAL             255U    /* 3 + 248 + 4                  */

/* CRC32 algorithm parameters */
#define FRAME_CRC32_POLY            0x04C11DB7U
#define FRAME_CRC32_INIT            0xFFFFFFFFU
#define FRAME_CRC32_XOROUT          0xFFFFFFFFU

/* ================================================================
 *  Return Codes
 * ================================================================ */

typedef enum
{
    FRAME_OK                  = 0,  /* Success                              */
    FRAME_ERR_NULL_PTR        = 1,  /* out_buf or out_len is NULL           */
    FRAME_ERR_BUF_TOO_SMALL   = 2,  /* out_cap < len + FRAME_OVERHEAD_BYTES */
    FRAME_ERR_PAYLOAD_TOO_BIG = 3   /* len > FRAME_MAX_PAYLOAD              */
} Frame_Status_t;

/* ================================================================
 *  Public API
 * ================================================================ */

/**
 * @brief  Build a complete frame in the caller's buffer.
 *
 *         Layout written:
 *           [SYNC=0xAA][TYPE][LEN][payload 0..LEN-1][CRC32 big-endian]
 *
 *         The CRC32 is computed over TYPE + LEN + PAYLOAD (LEN+2 bytes)
 *         using the MPEG-2-style algorithm with 32-iteration inner loop
 *         to match the teammate's reference implementation exactly.
 *
 * @param[in]   type     Frame type byte (e.g. FRAME_TYPE_RAW_IMU)
 * @param[in]   payload  Pointer to LEN bytes of payload data.
 *                       May be NULL if and only if len == 0.
 * @param[in]   len      Payload length in bytes, 0..FRAME_MAX_PAYLOAD.
 * @param[out]  out_buf  Output buffer. Must have capacity of at least
 *                       len + FRAME_OVERHEAD_BYTES.
 * @param[in]   out_cap  Capacity of out_buf in bytes.
 * @param[out]  out_len  On success, total bytes written to out_buf
 *                       (== len + FRAME_OVERHEAD_BYTES).
 *
 * @return FRAME_OK                   on success
 *         FRAME_ERR_NULL_PTR         if out_buf or out_len is NULL,
 *                                    or if payload is NULL and len > 0
 *         FRAME_ERR_BUF_TOO_SMALL    if out_cap < len + FRAME_OVERHEAD_BYTES
 *         FRAME_ERR_PAYLOAD_TOO_BIG  if len > FRAME_MAX_PAYLOAD
 *
 * @note   Stateless — no global mutation. Safe to call from multiple
 *         contexts as long as caller provides distinct buffers.
 *         Not ISR-safe purely on grounds of CRC computation cost
 *         (up to ~250 * 32 = 8000 iterations).
 */
Frame_Status_t Frame_Build(uint8_t        type,
                           const uint8_t *payload,
                           uint8_t        len,
                           uint8_t       *out_buf,
                           uint16_t       out_cap,
                           uint16_t      *out_len);

/**
 * @brief  Compute CRC32-MPEG2 (32-iteration variant) over a byte array.
 *
 *         Polynomial: 0x04C11DB7, Init: 0xFFFFFFFF, Final XOR: 0xFFFFFFFF.
 *         Inner loop runs 32 iterations per byte to match teammate's
 *         calculateCRC32() reference exactly.
 *
 *         Exposed publicly so that:
 *           - Receivers can validate inbound frames.
 *           - Unit tests can verify against fixed vectors.
 *
 * @param[in]  data  Pointer to input bytes. May be NULL if len == 0.
 * @param[in]  len   Number of bytes to process.
 *
 * @return CRC32 result. If data is NULL and len > 0, returns
 *         FRAME_CRC32_XOROUT (i.e. INIT ^ XOROUT = 0x00000000) —
 *         the same value as for empty input — caller should treat
 *         a NULL+len>0 call as a programming error.
 */
uint32_t Frame_CRC32(const uint8_t *data, uint16_t len);

#endif /* FRAME_H */
