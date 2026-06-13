#ifndef MBEDTLS_CONFIG_H
#define MBEDTLS_CONFIG_H

/* Minimal config for AES-GCM only */
#define MBEDTLS_HAVE_ASM
#define MBEDTLS_CIPHER_C
#define MBEDTLS_AES_C
#define MBEDTLS_GCM_C
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY

/* Exclude everything else to save flash */
// No RSA, ECC, SHA, TLS, etc.

#include "mbedtls/check_config.h"

#endif /* MBEDTLS_CONFIG_H */