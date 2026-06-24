/*****************************************************************************
 * log_payload.h — Log frame payload definition
 *
 * 10 bytes total, sent as FRAME_TYPE_LOG (0x04).
 * All multi-byte fields BIG-ENDIAN on the wire.
 *****************************************************************************/

#ifndef LOG_PAYLOAD_H
#define LOG_PAYLOAD_H

#include "STD_TYPES.h"

typedef struct {
    uint8_t  code;          /* Log_Code_t value */
    uint8_t  severity;      /* 0=INFO, 1=WARN, 2=ERROR, 3=FATAL */
    uint32_t timestamp_ms;  /* xTaskGetTickCount() at log time */
    uint32_t aux_data;      /* caller context */
} __attribute__((packed)) Log_Payload_t;

#define LOG_PAYLOAD_SIZE  10U

#endif /* LOG_PAYLOAD_H */