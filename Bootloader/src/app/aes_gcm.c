#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#define MBEDTLS_CONFIG_FILE "mbedtls/config.h"

#include "mbedtls/gcm.h"

#define AES_GCM_KEY_SIZE    16
#define AES_GCM_NONCE_SIZE  12
#define AES_GCM_TAG_SIZE    16

// Host key: 0724356978ABCDEF4123456789ABDDEF 
static const uint8_t AES_GCM_KEY[AES_GCM_KEY_SIZE] = {
    0x07, 0x24, 0x35, 0x69, 0x78, 0xAB, 0xCD, 0xEF,
    0x41, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xDD, 0xEF
};

// Base nonce: 000000000000000000000001
static const uint8_t AES_GCM_BASE_NONCE[AES_GCM_NONCE_SIZE] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01
};

/* Build nonce: base[0..7] + chunk_index(4 bytes, big-endian) */
static void build_nonce(uint32_t chunk_index, uint8_t *nonce_out){
    memcpy(nonce_out, AES_GCM_BASE_NONCE, 8);
    nonce_out[8]  =(chunk_index >> 24) & 0xFF;
    nonce_out[9]  =(chunk_index >> 16) & 0xFF;
    nonce_out[10] =(chunk_index >> 8)  & 0xFF;
    nonce_out[11] =(chunk_index)       & 0xFF;
}

// Encrypt(same as host)
static int test_encrypt(uint32_t chunk_index, const uint8_t *plaintext, size_t plaintext_len, uint8_t *ciphertext_out,uint8_t *tag_out){
    mbedtls_gcm_context ctx;
    mbedtls_gcm_init(&ctx);

    int ret = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, 
                                   AES_GCM_KEY, AES_GCM_KEY_SIZE * 8);
    if(ret != 0) return ret;

    uint8_t nonce[AES_GCM_NONCE_SIZE];
    build_nonce(chunk_index, nonce);

    uint8_t aad[4];
    memcpy(aad, &nonce[8], 4);

    ret = mbedtls_gcm_crypt_and_tag(&ctx, MBEDTLS_GCM_ENCRYPT,
                                    plaintext_len,
                                    nonce, AES_GCM_NONCE_SIZE,
                                    aad, sizeof(aad),
                                    plaintext, ciphertext_out,
                                    AES_GCM_TAG_SIZE, tag_out);

    mbedtls_gcm_free(&ctx);

    return ret;
}

// Decrypt(STM32 side)
bool AES_GCM_DecryptChunk(uint32_t chunk_index, const uint8_t *ciphertext, size_t ciphertext_len, const uint8_t *tag, uint8_t *plaintext_out){
    mbedtls_gcm_context ctx;
    mbedtls_gcm_init(&ctx);

    int ret = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, 
                                   AES_GCM_KEY, AES_GCM_KEY_SIZE * 8);
    if(ret != 0){
        mbedtls_gcm_free(&ctx);
        return false;
    }

    uint8_t nonce[AES_GCM_NONCE_SIZE];
    build_nonce(chunk_index, nonce);

    uint8_t aad[4];
    memcpy(aad, &nonce[8], 4);

    ret = mbedtls_gcm_auth_decrypt(&ctx, ciphertext_len,
                                   nonce, AES_GCM_NONCE_SIZE,
                                   aad, sizeof(aad),
                                   tag, AES_GCM_TAG_SIZE,
                                   ciphertext, plaintext_out);

    mbedtls_gcm_free(&ctx);

    return(ret == 0);
}

static bool test_decrypt(uint32_t chunk_index, const uint8_t *ciphertext, size_t ciphertext_len, const uint8_t *tag, uint8_t *plaintext_out){
    mbedtls_gcm_context ctx;
    mbedtls_gcm_init(&ctx);

    int ret = mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, 
                                   AES_GCM_KEY, AES_GCM_KEY_SIZE * 8);
    if(ret != 0){
        mbedtls_gcm_free(&ctx);
        return false;
    }

    uint8_t nonce[AES_GCM_NONCE_SIZE];
    build_nonce(chunk_index, nonce);

    uint8_t aad[4];
    memcpy(aad, &nonce[8], 4);

    ret = mbedtls_gcm_auth_decrypt(&ctx, ciphertext_len,
                                   nonce, AES_GCM_NONCE_SIZE,
                                   aad, sizeof(aad),
                                   tag, AES_GCM_TAG_SIZE,
                                   ciphertext, plaintext_out);

    mbedtls_gcm_free(&ctx);

    return(ret == 0);
}

// ============ PUBLIC TEST FUNCTION ============
#include "app/aes_gcm.h"

bool AES_GCM_RunTest(void){
    // Test 1: Full 2032-byte chunk
    uint8_t plaintext[2032];
    for(int i = 0; i < 2032; i++){
        plaintext[i] = i & 0xFF;
    }

    uint8_t ciphertext[2032];
    uint8_t tag[AES_GCM_TAG_SIZE];
    uint8_t decrypted[2032];

    int ret = test_encrypt(0, plaintext, 2032, ciphertext, tag);
    if(ret != 0){
        return false;
    }

    bool ok = test_decrypt(0, ciphertext, 2032, tag, decrypted);
    if(!ok){
        return false;
    }

    if(memcmp(plaintext, decrypted, 2032) == 0){
    } else {
        return false;
    }

    // Test 2: Wrong chunk index(should fail)
    ok = test_decrypt(1, ciphertext, 2032, tag, decrypted);
    if(ok){
        return false;
    }

    // Test 3: Corrupted tag(should fail)
    uint8_t bad_tag[AES_GCM_TAG_SIZE];
    memcpy(bad_tag, tag, AES_GCM_TAG_SIZE);
    bad_tag[0] ^= 0xFF;
    ok = test_decrypt(0, ciphertext, 2032, bad_tag, decrypted);
    if(ok){
        return false;
    }

    return true;
}