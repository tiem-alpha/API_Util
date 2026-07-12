/*
 * Minimal AES-128-GCM module extracted for standalone embedded use.
 *
 * This file keeps a small public API and intentionally avoids wolfSSL's
 * configuration layer, hardware acceleration paths, and dynamic allocation.
 */
#ifndef MY_CRYPTO_AES128_GCM_H
#define MY_CRYPTO_AES128_GCM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MY_AES128_GCM_KEY_SIZE   16u
#define MY_AES128_GCM_BLOCK_SIZE 16u
#define MY_AES128_GCM_TAG_SIZE   16u

enum {
    MY_AES128_GCM_OK = 0,
    MY_AES128_GCM_BAD_ARG = -1,
    MY_AES128_GCM_AUTH_FAIL = -2
};

int my_aes128_gcm_encrypt(const uint8_t key[MY_AES128_GCM_KEY_SIZE],
                          const uint8_t* iv, size_t iv_len,
                          const uint8_t* aad, size_t aad_len,
                          const uint8_t* plaintext, size_t plaintext_len,
                          uint8_t* ciphertext,
                          uint8_t tag[MY_AES128_GCM_TAG_SIZE]);

int my_aes128_gcm_decrypt(const uint8_t key[MY_AES128_GCM_KEY_SIZE],
                          const uint8_t* iv, size_t iv_len,
                          const uint8_t* aad, size_t aad_len,
                          const uint8_t* ciphertext, size_t ciphertext_len,
                          const uint8_t tag[MY_AES128_GCM_TAG_SIZE],
                          uint8_t* plaintext);

#ifdef __cplusplus
}
#endif

#endif /* MY_CRYPTO_AES128_GCM_H */
