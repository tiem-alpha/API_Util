/*
 * Minimal AES-128-GCM implementation for standalone embedded use.
 *
 * The AES/GCM structure follows the software path used by wolfCrypt's AES-GCM:
 * AES encrypts the all-zero block to derive H, CTR mode encrypts/decrypts the
 * payload, and GHASH authenticates AAD, ciphertext, and lengths.
 */
#include "aes128_gcm.h"

#include <string.h>

#define AES_ROUNDS 10u
#define AES_KEY_SCHEDULE_SIZE 176u

static const uint8_t sbox[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};

static const uint8_t rcon[10] = {
    0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1b,0x36
};

static uint8_t xtime(uint8_t x)
{
    return (uint8_t)((x << 1) ^ (((x >> 7) & 1u) * 0x1bu));
}

static void key_expand(const uint8_t key[16], uint8_t round_key[AES_KEY_SCHEDULE_SIZE])
{
    uint32_t i;
    uint8_t t[4];

    memcpy(round_key, key, 16);
    for (i = 4; i < 44; i++) {
        t[0] = round_key[(i - 1u) * 4u + 0u];
        t[1] = round_key[(i - 1u) * 4u + 1u];
        t[2] = round_key[(i - 1u) * 4u + 2u];
        t[3] = round_key[(i - 1u) * 4u + 3u];
        if ((i % 4u) == 0u) {
            uint8_t tmp = t[0];
            t[0] = (uint8_t)(sbox[t[1]] ^ rcon[(i / 4u) - 1u]);
            t[1] = sbox[t[2]];
            t[2] = sbox[t[3]];
            t[3] = sbox[tmp];
        }
        round_key[i * 4u + 0u] = (uint8_t)(round_key[(i - 4u) * 4u + 0u] ^ t[0]);
        round_key[i * 4u + 1u] = (uint8_t)(round_key[(i - 4u) * 4u + 1u] ^ t[1]);
        round_key[i * 4u + 2u] = (uint8_t)(round_key[(i - 4u) * 4u + 2u] ^ t[2]);
        round_key[i * 4u + 3u] = (uint8_t)(round_key[(i - 4u) * 4u + 3u] ^ t[3]);
    }
}

static void add_round_key(uint8_t state[16], const uint8_t* round_key)
{
    uint32_t i;
    for (i = 0; i < 16; i++) {
        state[i] ^= round_key[i];
    }
}

static void sub_bytes(uint8_t state[16])
{
    uint32_t i;
    for (i = 0; i < 16; i++) {
        state[i] = sbox[state[i]];
    }
}

static void shift_rows(uint8_t s[16])
{
    uint8_t t;

    t = s[1];  s[1]  = s[5];  s[5]  = s[9];  s[9]  = s[13]; s[13] = t;
    t = s[2];  s[2]  = s[10]; s[10] = t;
    t = s[6];  s[6]  = s[14]; s[14] = t;
    t = s[15]; s[15] = s[11]; s[11] = s[7];  s[7]  = s[3];  s[3]  = t;
}

static void mix_columns(uint8_t s[16])
{
    uint32_t i;
    for (i = 0; i < 4; i++) {
        uint8_t* c = &s[i * 4u];
        uint8_t a = c[0];
        uint8_t b = c[1];
        uint8_t d = c[2];
        uint8_t e = c[3];
        uint8_t x = (uint8_t)(a ^ b ^ d ^ e);
        uint8_t y = a;
        c[0] ^= (uint8_t)(x ^ xtime((uint8_t)(a ^ b)));
        c[1] ^= (uint8_t)(x ^ xtime((uint8_t)(b ^ d)));
        c[2] ^= (uint8_t)(x ^ xtime((uint8_t)(d ^ e)));
        c[3] ^= (uint8_t)(x ^ xtime((uint8_t)(e ^ y)));
    }
}

static void aes128_encrypt_block(const uint8_t round_key[AES_KEY_SCHEDULE_SIZE],
                                 const uint8_t in[16], uint8_t out[16])
{
    uint32_t round;
    uint8_t state[16];

    memcpy(state, in, 16);
    add_round_key(state, round_key);
    for (round = 1; round < AES_ROUNDS; round++) {
        sub_bytes(state);
        shift_rows(state);
        mix_columns(state);
        add_round_key(state, round_key + (round * 16u));
    }
    sub_bytes(state);
    shift_rows(state);
    add_round_key(state, round_key + 160u);
    memcpy(out, state, 16);
}

static void inc32(uint8_t ctr[16])
{
    uint32_t n = ((uint32_t)ctr[12] << 24) |
                 ((uint32_t)ctr[13] << 16) |
                 ((uint32_t)ctr[14] << 8)  |
                 ((uint32_t)ctr[15]);
    n++;
    ctr[12] = (uint8_t)(n >> 24);
    ctr[13] = (uint8_t)(n >> 16);
    ctr[14] = (uint8_t)(n >> 8);
    ctr[15] = (uint8_t)n;
}

static void xor_block(uint8_t dst[16], const uint8_t src[16])
{
    uint32_t i;
    for (i = 0; i < 16; i++) {
        dst[i] ^= src[i];
    }
}

static void ghash_mul(uint8_t x[16], const uint8_t h[16])
{
    uint8_t z[16] = {0};
    uint8_t v[16];
    uint32_t i;

    memcpy(v, h, 16);
    for (i = 0; i < 128; i++) {
        uint8_t bit = (uint8_t)((x[i / 8u] >> (7u - (i % 8u))) & 1u);
        uint8_t lsb;
        int j;

        if (bit != 0u) {
            xor_block(z, v);
        }

        lsb = (uint8_t)(v[15] & 1u);
        for (j = 15; j > 0; j--) {
            v[j] = (uint8_t)((v[j] >> 1) | ((v[j - 1] & 1u) << 7));
        }
        v[0] >>= 1;
        if (lsb != 0u) {
            v[0] ^= 0xe1u;
        }
    }
    memcpy(x, z, 16);
}

static void ghash_update(uint8_t y[16], const uint8_t h[16],
                         const uint8_t* data, size_t data_len)
{
    while (data_len >= 16u) {
        xor_block(y, data);
        ghash_mul(y, h);
        data += 16;
        data_len -= 16;
    }

    if (data_len > 0u) {
        uint8_t block[16] = {0};
        memcpy(block, data, data_len);
        xor_block(y, block);
        ghash_mul(y, h);
    }
}

static void store64_be(uint8_t out[8], uint64_t v)
{
    out[0] = (uint8_t)(v >> 56);
    out[1] = (uint8_t)(v >> 48);
    out[2] = (uint8_t)(v >> 40);
    out[3] = (uint8_t)(v >> 32);
    out[4] = (uint8_t)(v >> 24);
    out[5] = (uint8_t)(v >> 16);
    out[6] = (uint8_t)(v >> 8);
    out[7] = (uint8_t)v;
}

static void ghash_finish(uint8_t y[16], const uint8_t h[16],
                         uint64_t aad_len, uint64_t text_len)
{
    uint8_t block[16];

    store64_be(block, aad_len * 8u);
    store64_be(block + 8, text_len * 8u);
    xor_block(y, block);
    ghash_mul(y, h);
}

static void make_j0(const uint8_t round_key[AES_KEY_SCHEDULE_SIZE],
                    const uint8_t h[16], const uint8_t* iv, size_t iv_len,
                    uint8_t j0[16])
{
    if (iv_len == 12u) {
        memcpy(j0, iv, 12);
        j0[12] = 0;
        j0[13] = 0;
        j0[14] = 0;
        j0[15] = 1;
    }
    else {
        uint8_t y[16] = {0};
        (void)round_key;
        ghash_update(y, h, iv, iv_len);
        ghash_finish(y, h, 0u, (uint64_t)iv_len);
        memcpy(j0, y, 16);
    }
}

static void ctr_crypt(const uint8_t round_key[AES_KEY_SCHEDULE_SIZE],
                      const uint8_t j0[16], const uint8_t* in,
                      size_t len, uint8_t* out)
{
    uint8_t ctr[16];
    uint8_t stream[16];
    size_t i;

    memcpy(ctr, j0, 16);
    inc32(ctr);

    while (len > 0u) {
        size_t block_len = (len < 16u) ? len : 16u;
        aes128_encrypt_block(round_key, ctr, stream);
        for (i = 0; i < block_len; i++) {
            out[i] = (uint8_t)(in[i] ^ stream[i]);
        }
        in += block_len;
        out += block_len;
        len -= block_len;
        inc32(ctr);
    }
}

static int args_ok(const uint8_t* key, const uint8_t* iv, size_t iv_len,
                   const uint8_t* aad, size_t aad_len, const uint8_t* in,
                   size_t in_len, const uint8_t* out)
{
    if (key == NULL || iv == NULL || iv_len == 0u) {
        return 0;
    }
    if ((aad_len > 0u && aad == NULL) || (in_len > 0u && (in == NULL || out == NULL))) {
        return 0;
    }
    return 1;
}

static void calc_tag(const uint8_t round_key[AES_KEY_SCHEDULE_SIZE],
                     const uint8_t h[16], const uint8_t j0[16],
                     const uint8_t* aad, size_t aad_len,
                     const uint8_t* ciphertext, size_t ciphertext_len,
                     uint8_t tag[16])
{
    uint8_t y[16] = {0};
    uint8_t e_j0[16];

    ghash_update(y, h, aad, aad_len);
    ghash_update(y, h, ciphertext, ciphertext_len);
    ghash_finish(y, h, (uint64_t)aad_len, (uint64_t)ciphertext_len);
    aes128_encrypt_block(round_key, j0, e_j0);
    memcpy(tag, y, 16);
    xor_block(tag, e_j0);
}

int my_aes128_gcm_encrypt(const uint8_t key[MY_AES128_GCM_KEY_SIZE],
                          const uint8_t* iv, size_t iv_len,
                          const uint8_t* aad, size_t aad_len,
                          const uint8_t* plaintext, size_t plaintext_len,
                          uint8_t* ciphertext,
                          uint8_t tag[MY_AES128_GCM_TAG_SIZE])
{
    uint8_t round_key[AES_KEY_SCHEDULE_SIZE];
    uint8_t zero[16] = {0};
    uint8_t h[16];
    uint8_t j0[16];

    if (!args_ok(key, iv, iv_len, aad, aad_len, plaintext, plaintext_len, ciphertext) ||
        tag == NULL) {
        return MY_AES128_GCM_BAD_ARG;
    }

    key_expand(key, round_key);
    aes128_encrypt_block(round_key, zero, h);
    make_j0(round_key, h, iv, iv_len, j0);
    ctr_crypt(round_key, j0, plaintext, plaintext_len, ciphertext);
    calc_tag(round_key, h, j0, aad, aad_len, ciphertext, plaintext_len, tag);

    return MY_AES128_GCM_OK;
}

int my_aes128_gcm_decrypt(const uint8_t key[MY_AES128_GCM_KEY_SIZE],
                          const uint8_t* iv, size_t iv_len,
                          const uint8_t* aad, size_t aad_len,
                          const uint8_t* ciphertext, size_t ciphertext_len,
                          const uint8_t tag[MY_AES128_GCM_TAG_SIZE],
                          uint8_t* plaintext)
{
    uint8_t round_key[AES_KEY_SCHEDULE_SIZE];
    uint8_t zero[16] = {0};
    uint8_t h[16];
    uint8_t j0[16];
    uint8_t expected[16];
    uint8_t diff = 0;
    uint32_t i;

    if (!args_ok(key, iv, iv_len, aad, aad_len, ciphertext, ciphertext_len, plaintext) ||
        tag == NULL) {
        return MY_AES128_GCM_BAD_ARG;
    }

    key_expand(key, round_key);
    aes128_encrypt_block(round_key, zero, h);
    make_j0(round_key, h, iv, iv_len, j0);
    calc_tag(round_key, h, j0, aad, aad_len, ciphertext, ciphertext_len, expected);

    for (i = 0; i < 16u; i++) {
        diff |= (uint8_t)(expected[i] ^ tag[i]);
    }
    if (diff != 0u) {
        if (ciphertext_len > 0u) {
            memset(plaintext, 0, ciphertext_len);
        }
        return MY_AES128_GCM_AUTH_FAIL;
    }

    ctr_crypt(round_key, j0, ciphertext, ciphertext_len, plaintext);
    return MY_AES128_GCM_OK;
}
