// Copyright 2016 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "2sha.h"
#include "cryptoc/p256_ecdsa.h"
#include "cryptoc/p256.h"

// Compute k based on given {key, message} pair, 0 < k < n.
static void determine_k(const p256_int *key, const p256_int *message,
			uint8_t *tweak, p256_int *k)
{
	p256_int p;

	do {
		// SHA256 prng pick until p <= SECP256r1_n - 2,
		// in a (re)pick and test fashion for a clean distribution.

		struct vb2_sha256_context ctx;
		uint8_t tmp[P256_NBYTES];

		vb2_sha256_init(&ctx, VB2_HASH_SHA256);

		p256_to_bin(key, tmp);
		vb2_sha256_update(&ctx, tmp, P256_NBYTES);

		vb2_sha256_update(&ctx, tweak, 1);

		p256_to_bin(message, tmp);
		vb2_sha256_update(&ctx, tmp, P256_NBYTES);

		vb2_sha256_finalize(&ctx, tmp, VB2_HASH_SHA256);
		p256_from_bin(tmp, &p);
	} while (p256_cmp(&p, &SECP256r1_nMin2) > 0);

	p256_add_d(&p, 1, k); // 0 < k < SECP256r1_n
	p256_clear(&p);
}

void p256_ecdsa_sign(const p256_int *key, const p256_int *message, p256_int *r,
		     p256_int *s)
{
	uint8_t tweak = 'A';
	p256_digit top;

	for (;;) {
		p256_int k, kinv;

		determine_k(key, message, &tweak, &k);
		p256_base_point_mul(&k, r, s);
		p256_mod(&SECP256r1_n, r, r);

		// Make sure r != 0
		if (p256_is_zero(r))
			continue;

		p256_modmul(&SECP256r1_n, r, 0, key, s);
		top = p256_add(s, message, s);
		p256_modinv(&SECP256r1_n, &k, &kinv);
		p256_modmul(&SECP256r1_n, &kinv, top, s, s);

		// (Attempt to) clear stack state.
		p256_clear(&k);
		p256_clear(&kinv);

		// Make sure s != 0
		if (p256_is_zero(s))
			continue;

		break;
	}
}

int p256_ecdsa_verify(const p256_int *key_x, const p256_int *key_y,
		      const p256_int *message, const p256_int *r,
		      const p256_int *s)
{
	p256_int u, v;

	// Check public key.
	if (!p256_is_valid_point(key_x, key_y))
		return 0;

	// Check r and s are != 0 % n.
	p256_mod(&SECP256r1_n, r, &u);
	p256_mod(&SECP256r1_n, s, &v);
	if (p256_is_zero(&u) || p256_is_zero(&v))
		return 0;

	p256_modinv_vartime(&SECP256r1_n, s, &v);
	p256_modmul(&SECP256r1_n, message, 0, &v, &u); // message / s % n
	p256_modmul(&SECP256r1_n, r, 0, &v, &v);       // r / s % n

	p256_points_mul_vartime(&u, &v, key_x, key_y, &u, &v);

	p256_mod(&SECP256r1_n, &u, &u); // (x coord % p) % n
	return p256_cmp(r, &u) == 0;
}
