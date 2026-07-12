/**
 * @file x25519.h
 * @brief Minimal ECDH X25519 API.
 *
 * @author nguyentiem
 *
 * This module implements X25519 over Curve25519 as specified by RFC 7748.
 *
 * Public API:
 * - x25519_generate_keypair(): create an ECDH private/public key pair.
 * - x25519_shared_key(): derive a shared key from local private key and peer
 *   public key.
 *
 * Buffer format:
 * - X25519 private key: 32 bytes, little-endian scalar bytes. The scalar is
 *   clamped internally by the X25519 primitive.
 * - X25519 public key: 32 bytes, little-endian u-coordinate.
 * - X25519 shared key: 32 bytes, little-endian u-coordinate result.
 *
 * Memory policy:
 * - Caller owns all input/output buffers.
 * - No heap allocation is used.
 * - No large precomputed RAM tables are used.
 */
#ifndef MY_CRYPTO_ECDH_X25519_H
#define MY_CRYPTO_ECDH_X25519_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Size in bytes of an X25519 private key, public key or shared key. */
#define X25519_SIZE 32u

/**
 * @brief Random byte generator callback.
 *
 * @param[in,out] ctx User-provided RNG context. May be NULL if the callback
 *                    does not need context.
 * @param[out] out Output buffer to fill with random bytes.
 * @param[in] out_len Number of random bytes requested.
 *
 * @return 0 on success.
 * @return Non-zero on RNG failure.
 *
 * Security requirement: for production key generation, this callback must be a
 * cryptographically secure random number generator.
 */
typedef int (*x25519_random_func)(void *ctx, uint8_t *out, size_t out_len);

/**
 * @brief Generate an X25519 ECDH key pair.
 *
 * This function fills @p private_key with 32 random bytes from @p rng and then
 * computes @p public_key = X25519(private_key, basepoint).
 *
 * @param[out] private_key 32-byte output buffer for the generated private key.
 *                         The stored bytes are not pre-clamped; clamping is
 *                         applied internally whenever X25519 is computed.
 * @param[out] public_key 32-byte output buffer for the generated public key.
 * @param[in] rng Random byte generator callback.
 * @param[in,out] rng_ctx User context passed to @p rng. May be NULL if the RNG
 *                        callback supports NULL context.
 *
 * @return 0 on success.
 * @return -1 if an argument is NULL, the RNG fails, or public-key derivation
 *         fails.
 *
 * @note The caller should keep @p private_key secret and may transmit
 *       @p public_key to the peer.
 */
int x25519_generate_keypair(uint8_t private_key[X25519_SIZE],
                            uint8_t public_key[X25519_SIZE],
                            x25519_random_func rng,
                            void *rng_ctx);

/**
 * @brief Derive an X25519 ECDH shared key.
 *
 * Computes:
 *
 *     shared_key = X25519(private_key, peer_public_key)
 *
 * The function rejects an all-zero result, which normally means the peer public
 * key is weak or invalid for ECDH use.
 *
 * @param[in] private_key Local 32-byte X25519 private key.
 * @param[in] peer_public_key Peer 32-byte X25519 public key.
 * @param[out] shared_key 32-byte output buffer for the derived shared key.
 *
 * @return 0 on success.
 * @return -1 if an argument is NULL or the internal primitive fails.
 * @return -2 if the derived shared key is all zero.
 *
 * @note Feed @p shared_key into a KDF before using it as an encryption key.
 */
int x25519_shared_key(const uint8_t private_key[X25519_SIZE],
                      const uint8_t peer_public_key[X25519_SIZE],
                      uint8_t shared_key[X25519_SIZE]);

#ifdef __cplusplus
}
#endif

#endif
