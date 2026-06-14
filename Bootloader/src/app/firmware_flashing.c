#include "app/firmware_flashing.h"
#include "app/aes_gcm.h"
#include "interface/MCAL/flash.h"
#include <string.h>
#include <stdio.h>

static uint8_t cipher_buffer[OTA_CHUNK_ENCRYPTED_SIZE];
static uint8_t plaintext_buffer[OTA_CHUNK_PLAINTEXT_SIZE];

static struct {
    OTA_State_t state;
    uint32_t chunk_index;           // Which chunk we're on (for nonce)
    uint16_t cipher_count;          // Bytes in cipher_buffer
    uint32_t flash_addr;            // Next flash write address
} ota;

static bool decrypt_and_flash(uint16_t encrypted_len){
    if (encrypted_len < 16) {
        return false;
    }
    
    uint16_t cipher_len = encrypted_len - 16;
    uint8_t *cipher = cipher_buffer;
    uint8_t *tag = cipher_buffer + cipher_len;
    
    bool ok = AES_GCM_DecryptChunk(ota.chunk_index, cipher, cipher_len, tag, plaintext_buffer);
    if (!ok) {
        return false;
    }
    
    // Write plaintext to flash
    uint32_t words = (cipher_len + 3) / 4;
    if (FLASH_Write(ota.flash_addr, (uint32_t *)plaintext_buffer, words) != STD_SUCCESS) {
        return false;
    }
    
    ota.flash_addr += cipher_len;
    ota.chunk_index++;
    
    return true;
}

void OTA_Init(void){
    memset(&ota, 0, sizeof(ota));
    ota.state = OTA_STATE_ACCUMULATING;
    ota.flash_addr = FLASH_APP_START;
}

bool OTA_ReceivePacket(const uint8_t *payload, uint8_t payload_len){
    if(ota.state == OTA_STATE_ERROR || ota.state != OTA_STATE_ACCUMULATING || payload_len == 0 || payload_len > OTA_PACKET_SIZE){
        return false;
    }
    
    // Check buffer space
    if(ota.cipher_count + payload_len > OTA_CHUNK_ENCRYPTED_SIZE){
        ota.state = OTA_STATE_ERROR;
        return false;
    }
    
    // Append to cipher buffer
    memcpy(cipher_buffer + ota.cipher_count, payload, payload_len);
    ota.cipher_count += payload_len;
    
    // Condition 1: Buffer full (2048 bytes) → decrypt and flush
    if (ota.cipher_count >= OTA_CHUNK_ENCRYPTED_SIZE) {
        if (!decrypt_and_flash(OTA_CHUNK_ENCRYPTED_SIZE)) {
            ota.state = OTA_STATE_ERROR;
            return false;
        }
        ota.cipher_count = 0;
        memset(cipher_buffer, 0, sizeof(cipher_buffer));
    }
    
    // Condition 2: Payload < 64 bytes → this is the last packet, auto-finish
    if (payload_len < OTA_PACKET_SIZE) {        
        if (ota.cipher_count > 0) {
            if (!decrypt_and_flash(ota.cipher_count)) {
                ota.state = OTA_STATE_ERROR;
                return false;
            }
        }
        
        ota.state = OTA_STATE_COMPLETE;
    }
    
    return true;
}

bool OTA_Finish(void){
    if (ota.state == OTA_STATE_ERROR){
        return false;
    }
    if (ota.cipher_count > 0) {
        if (!decrypt_and_flash(ota.cipher_count)) {
            ota.state = OTA_STATE_ERROR;
            return false;
        }
    }
    
    ota.state = OTA_STATE_COMPLETE;
    
    return true;
}

OTA_State_t OTA_GetState(void){
    return ota.state;
}
