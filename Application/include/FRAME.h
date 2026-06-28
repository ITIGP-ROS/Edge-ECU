#ifndef FRAME_H
#define FRAME_H

#include "STD_TYPES.h"

/* Frame TYPE codes — STM32 originating frames */
#define FRAME_TYPE_CLASSIFICATION   0x01U
#define FRAME_TYPE_TEMPERATURE      0x02U  /* LM35 temp: 2-byte temp_x10 (BE) + 4-byte timestamp_ms (BE) */
#define FRAME_TYPE_HEARTBEAT        0x03U
#define FRAME_TYPE_LOG              0x04U 
#define FRAME_TYPE_BL_ACK           0xFFU

/* Frame TYPE codes — ESP32 originating frames (received with 0xAA SYNC) */
#define FRAME_TYPE_BL_ENTER         0xFEU
#define FRAME_TYPE_BL_DATA          0xFDU
#define FRAME_TYPE_BL_VERIFY        0xFCU

/* ECU identifiers */
#define ECU_ID_STM32_NODE1          0x01U
#define ECU_ID_ESP32_GATEWAY        0x02U

/* Frame layout constants */
#define FRAME_HEADER_BYTES          3U      /* LEN + TYPE + ECU_ID         */
#define FRAME_CRC_BYTES             4U
#define FRAME_OVERHEAD_BYTES        7U      /* LEN + TYPE + ECU + CRC      */
#define FRAME_MAX_PAYLOAD           248U    /* keeps total frame <= 255    */
#define FRAME_MAX_TOTAL             255U

/* Inbound (ESP32 → STM32) SYNC byte — used by the receiver state machine */
#define FRAME_INBOUND_SYNC_BYTE     0xAAU

/* CRC32 algorithm constants (unchanged) */
#define FRAME_CRC32_POLY            0x04C11DB7U
#define FRAME_CRC32_INIT            0xFFFFFFFFU
#define FRAME_CRC32_XOROUT          0xFFFFFFFFU

typedef enum {
    FRAME_OK                  = 0,
    FRAME_ERR_NULL_PTR        = 1,
    FRAME_ERR_BUF_TOO_SMALL   = 2,
    FRAME_ERR_PAYLOAD_TOO_BIG = 3
} Frame_Status_t;

/**
 * @brief  Build a complete frame in the caller's buffer.
 *
 * Layout written (no SYNC byte — GPIO sync is used externally):
 *   [LEN][TYPE][ECU_ID][payload 0..len-1][CRC32 big-endian, 4 bytes]
 *
 * LEN = len + 6
 * CRC32 is computed over [TYPE, ECU_ID, payload[0..len-1]]
 * (i.e., starting at out_buf[1] for len + 2 bytes)
 *
 * @param[in]   type     Frame type byte (e.g. FRAME_TYPE_CLASSIFICATION)
 * @param[in]   ecu_id   Source ECU identifier (e.g. ECU_ID_STM32_NODE1)
 * @param[in]   payload  Pointer to len bytes of payload data.
 *                       May be NULL if and only if len == 0.
 * @param[in]   len      Payload length, 0..FRAME_MAX_PAYLOAD.
 * @param[out]  out_buf  Output buffer. Must have capacity of at least
 *                       len + FRAME_OVERHEAD_BYTES.
 * @param[in]   out_cap  Capacity of out_buf in bytes.
 * @param[out]  out_len  On success, total bytes written.
 *
 * @return FRAME_OK / FRAME_ERR_NULL_PTR / FRAME_ERR_BUF_TOO_SMALL /
 *         FRAME_ERR_PAYLOAD_TOO_BIG.
 */
Frame_Status_t Frame_Build(uint8_t        type,
                           uint8_t        ecu_id,
                           const uint8_t *payload,
                           uint8_t        len,
                           uint8_t       *out_buf,
                           uint16_t       out_cap,
                           uint16_t      *out_len);

/**
 * @brief  Compute CRC32-MPEG2 (32-iteration variant) over a buffer.
 *
 *         Implementation is identical to the previous version.
 *         Exposed for receivers and unit tests.
 */
uint32_t Frame_CRC32(const uint8_t *data, uint16_t len);

#endif /* FRAME_H */
