/*
 * Standalone ECDH X25519 implementation.
 *
 * Author: nguyentiem
 *
 * This file intentionally keeps only the X25519 path. It uses the Montgomery
 * ladder directly for Curve25519 so it can be copied as a small independent
 * module.
 *
 * RAM policy: no heap allocation and no precomputed tables. The Montgomery
 * ladder uses a small fixed stack frame and wipes secret temporaries.
 */
#include "x25519.h"

#include <string.h>

typedef long long fe[16];

static const uint8_t x25519_basepoint[X25519_SIZE] = { 9 };

static void memzero(void *p, size_t n)
{
	volatile uint8_t *v = (volatile uint8_t *)p;

	while (n-- != 0u) {
		*v++ = 0;
	}
}

static int is_all_zero(const uint8_t in[X25519_SIZE])
{
	uint8_t acc = 0;
	size_t i;

	for (i = 0; i < X25519_SIZE; i++) {
		acc |= in[i];
	}

	return acc == 0;
}

static void car25519(fe o)
{
	int i;
	long long c;

	for (i = 0; i < 16; i++) {
		o[i] += 1LL << 16;
		c = o[i] >> 16;
		o[(i + 1) & 15] += c - 1 + 37 * (c - 1) * (i == 15);
		o[i] -= c << 16;
	}
}

static void sel25519(fe p, fe q, int b)
{
	long long t;
	int i;
	long long c = ~(long long)(b - 1);

	for (i = 0; i < 16; i++) {
		t = c & (p[i] ^ q[i]);
		p[i] ^= t;
		q[i] ^= t;
	}
}

static void pack25519(uint8_t *o, const fe n)
{
	int i;
	int j;
	int b;
	fe m;
	fe t;

	for (i = 0; i < 16; i++) {
		t[i] = n[i];
	}

	car25519(t);
	car25519(t);
	car25519(t);

	for (j = 0; j < 2; j++) {
		m[0] = t[0] - 0xffed;
		for (i = 1; i < 15; i++) {
			m[i] = t[i] - 0xffff - ((m[i - 1] >> 16) & 1);
			m[i - 1] &= 0xffff;
		}
		m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1);
		b = (int)((m[15] >> 16) & 1);
		m[14] &= 0xffff;
		sel25519(t, m, 1 - b);
	}

	for (i = 0; i < 16; i++) {
		o[2 * i] = (uint8_t)(t[i] & 0xff);
		o[2 * i + 1] = (uint8_t)(t[i] >> 8);
	}
}

static void unpack25519(fe o, const uint8_t *n)
{
	int i;

	for (i = 0; i < 16; i++) {
		o[i] = (long long)n[2 * i] + ((long long)n[2 * i + 1] << 8);
	}
	o[15] &= 0x7fff;
}

static void fe_add(fe o, const fe a, const fe b)
{
	int i;

	for (i = 0; i < 16; i++) {
		o[i] = a[i] + b[i];
	}
}

static void fe_sub(fe o, const fe a, const fe b)
{
	int i;

	for (i = 0; i < 16; i++) {
		o[i] = a[i] - b[i];
	}
}

static void fe_mul(fe o, const fe a, const fe b)
{
	long long t[31];
	int i;
	int j;

	for (i = 0; i < 31; i++) {
		t[i] = 0;
	}
	for (i = 0; i < 16; i++) {
		for (j = 0; j < 16; j++) {
			t[i + j] += a[i] * b[j];
		}
	}
	for (i = 0; i < 15; i++) {
		t[i] += 38 * t[i + 16];
	}
	for (i = 0; i < 16; i++) {
		o[i] = t[i];
	}

	car25519(o);
	car25519(o);
}

static void fe_mul121665(fe o, const fe a)
{
	long long t[16];
	int i;

	t[0] = a[0] * 0xdb41 + 38 * a[15];
	for (i = 1; i < 16; i++) {
		t[i] = a[i] * 0xdb41 + a[i - 1];
	}
	for (i = 0; i < 16; i++) {
		o[i] = t[i];
	}

	car25519(o);
	car25519(o);
}

static void fe_square(fe o, const fe a)
{
	long long t[31];
	long long product;
	int i;
	int j;

	for (i = 0; i < 31; i++) {
		t[i] = 0;
	}
	for (i = 0; i < 16; i++) {
		for (j = i; j < 16; j++) {
			product = a[i] * a[j];
			if (i != j) {
				product += product;
			}
			t[i + j] += product;
		}
	}
	for (i = 0; i < 15; i++) {
		t[i] += 38 * t[i + 16];
	}
	for (i = 0; i < 16; i++) {
		o[i] = t[i];
	}

	car25519(o);
	car25519(o);
}

static void inv25519(fe o, const fe i)
{
	fe c;
	int a;

	for (a = 0; a < 16; a++) {
		c[a] = i[a];
	}
	for (a = 253; a >= 0; a--) {
		fe_square(c, c);
		if (a != 2 && a != 4) {
			fe_mul(c, c, i);
		}
	}
	for (a = 0; a < 16; a++) {
		o[a] = c[a];
	}
}

static int x25519_raw(const uint8_t k[X25519_SIZE],
                      const uint8_t u[X25519_SIZE],
                      uint8_t res[X25519_SIZE])
{
	uint8_t z[X25519_SIZE];
	fe x;
	fe a;
	fe b;
	fe c;
	fe d;
	fe e;
	fe f;
	int i;
	int r;
	int swap;

	if (k == NULL || u == NULL || res == NULL) {
		return -1;
	}

	memcpy(z, k, sizeof(z));
	z[0] &= 248;
	z[31] &= 127;
	z[31] |= 64;

	unpack25519(x, u);

	for (i = 0; i < 16; i++) {
		b[i] = x[i];
		d[i] = a[i] = c[i] = 0;
	}
	a[0] = 1;
	d[0] = 1;
	swap = 0;

	for (i = 254; i >= 0; i--) {
		r = (z[i >> 3] >> (i & 7)) & 1;
		swap ^= r;
		sel25519(a, b, swap);
		sel25519(c, d, swap);
		swap = r;
		fe_add(e, a, c);
		fe_sub(a, a, c);
		fe_add(c, b, d);
		fe_sub(b, b, d);
		fe_square(d, e);
		fe_square(f, a);
		fe_mul(a, c, a);
		fe_mul(c, b, e);
		fe_add(e, a, c);
		fe_sub(a, a, c);
		fe_square(b, a);
		fe_sub(c, d, f);
		fe_mul121665(a, c);
		fe_add(a, a, d);
		fe_mul(c, c, a);
		fe_mul(a, d, f);
		fe_mul(d, b, x);
		fe_square(b, e);
	}
	sel25519(a, b, swap);
	sel25519(c, d, swap);

	inv25519(c, c);
	fe_mul(a, a, c);
	pack25519(res, a);

	memzero(z, sizeof(z));
	memzero(a, sizeof(a));
	memzero(b, sizeof(b));
	memzero(c, sizeof(c));
	memzero(d, sizeof(d));
	memzero(e, sizeof(e));
	memzero(f, sizeof(f));
	memzero(x, sizeof(x));

	return 0;
}

int x25519_generate_keypair(uint8_t private_key[X25519_SIZE],
                            uint8_t public_key[X25519_SIZE],
                            x25519_random_func rng,
                            void *rng_ctx)
{
	if (private_key == NULL || public_key == NULL || rng == NULL) {
		return -1;
	}

	if (rng(rng_ctx, private_key, X25519_SIZE) != 0) {
		return -1;
	}
	return x25519_raw(private_key, x25519_basepoint, public_key);
}

int x25519_shared_key(const uint8_t private_key[X25519_SIZE],
                      const uint8_t peer_public_key[X25519_SIZE],
                      uint8_t shared_key[X25519_SIZE])
{
	int ret;

	ret = x25519_raw(private_key, peer_public_key, shared_key);
	if (ret != 0) {
		return ret;
	}
	if (is_all_zero(shared_key)) {
		return -2;
	}

	return 0;
}
