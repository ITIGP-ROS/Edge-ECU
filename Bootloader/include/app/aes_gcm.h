#ifndef AES_GCM_H
#define AES_GCM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define AES_GCM_KEY_SIZE    16
#define AES_GCM_NONCE_SIZE  12
#define AES_GCM_TAG_SIZE    16

/* Your host key: "0724356978ABCDEF4123456789ABDDEF" */
extern const uint8_t AES_GCM_KEY[AES_GCM_KEY_SIZE];

/* Base nonce: "000000000000000000000001" */
extern const uint8_t AES_GCM_BASE_NONCE[AES_GCM_NONCE_SIZE];

/**
 * @brief Decrypt a firmware chunk using AES-GCM
 * 
 * @param chunk_index   Chunk index (used for nonce derivation)
 * @param ciphertext    Encrypted data (without tag)
 * @param ciphertext_len Length of ciphertext
 * @param tag           16-byte authentication tag
 * @param plaintext_out Output buffer (must be ciphertext_len bytes)
 * @return true if tag verification passed, false otherwise
 */
bool AES_GCM_DecryptChunk(uint32_t chunk_index,
                          const uint8_t *ciphertext,
                          size_t ciphertext_len,
                          const uint8_t *tag,
                          uint8_t *plaintext_out);

/**
 * @brief Test function - verify AES-GCM works correctly
 * @return true if all tests pass
 */
bool AES_GCM_RunTest(void);

#endif /* AES_GCM_H */