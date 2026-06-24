/*****************************************************************************
 * frame_request.h — Unified UART transmit request
 *
 * Senders push a FrameRequest_t to g_frame_queue. Thread 3 (UART TX) pops
 * it, runs Frame_Build, and transmits via DMA.
 *
 * Sized to fit the largest expected payload (heartbeat = 24 bytes) with
 * headroom. Padded internally for future use (logs, raw IMU snapshots).
 *****************************************************************************/

#ifndef FRAME_REQUEST_H
#define FRAME_REQUEST_H

#include "STD_TYPES.h"

/* Maximum payload size carried by a single frame request.
 * Sized for the heartbeat payload (24 bytes) plus headroom. */
#define FRAME_REQ_MAX_PAYLOAD   32U

/**
 * @brief  Unified UART transmit request.
 *
 *         Filled by any sender task and queued to the UART TX task.
 *         Total size: 35 bytes (1 + 1 + 1 + 32).
 */
typedef struct {
    uint8_t  type;                          /* FRAME_TYPE_* code        */
    uint8_t  ecu_id;                        /* ECU_ID_* code            */
    uint8_t  length;                        /* actual payload length    */
    uint8_t  payload[FRAME_REQ_MAX_PAYLOAD];/* application data         */
} FrameRequest_t;

/* Compile-time guard: make sure no future expansion exceeds the queue
 * memory budget. Allows safe up to 32-byte payloads. */
#if (FRAME_REQ_MAX_PAYLOAD < 24U)
#  error "FRAME_REQ_MAX_PAYLOAD must be >= 24 to fit Heartbeat payload"
#endif

#endif /* FRAME_REQUEST_H */
