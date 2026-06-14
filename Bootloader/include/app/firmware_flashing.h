#ifndef FIRMWARE_FLASHING_H
#define FIRMWARE_FLASHING_H

#include <stdint.h>
#include <stdbool.h>

#define OTA_CHUNK_ENCRYPTED_SIZE    2048   // 2032 cipher + 16 tag
#define OTA_CHUNK_PLAINTEXT_SIZE    2032
#define OTA_PACKET_SIZE             64

#define FLASH_APP_START             0x08008000

// OTA State
typedef enum {
    OTA_STATE_IDLE = 0,
    OTA_STATE_ACCUMULATING,
    OTA_STATE_ERROR,
    OTA_STATE_COMPLETE
} OTA_State_t;

void OTA_Init(void);

// Receive one packet. Returns true=OK, false=error
bool OTA_ReceivePacket(const uint8_t *payload, uint8_t payload_len);

// Explicit finish (EN signal)
bool OTA_Finish(void);

OTA_State_t OTA_GetState(void);

#endif