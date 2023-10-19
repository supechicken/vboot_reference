#ifndef _KEY_POSITIVE_INVERSE_MODULUS_H
#define _KEY_POSITIVE_INVERSE_MODULUS_H

#include "2common.h"

#define N_KEY_ENTRIES 10

static uint32_t inv_mod;
static struct pub_key_data{
	uint32_t pub_key_n0;
	uint32_t n0inv;
	uint32_t p0inv;
} INVMOD[] = {
	{.pub_key_n0 = 0xd525782f, .n0inv = 0x1d9d3131, .p0inv = 0xe262cecf},
	{.pub_key_n0 = 0x5358f0a3, .n0inv = 0x18cebcf5, .p0inv = 0xe731430b},
	{.pub_key_n0 = 0x2d3b9259, .n0inv = 0x69d56a17, .p0inv = 0x962a95e9},
	{.pub_key_n0 = 0xff3932c9, .n0inv = 0x8cc87887, .p0inv = 0x73378779},
	{.pub_key_n0 = 0xf5377797, .n0inv = 0xe0cd87d9, .p0inv = 0x1f327827},
	{.pub_key_n0 = 0xf862d1d, .n0inv = 0x414002cb, .p0inv = 0xbebffd35},
	{.pub_key_n0 = 0x2d3b9259, .n0inv = 0x69d56a17, .p0inv = 0x962a95e9},
	{.pub_key_n0 = 0xdf3d7fef, .n0inv = 0xfe70f1, .p0inv = 0xff018f0f},
	{.pub_key_n0 = 0x27d8e08b, .n0inv = 0x52f78dd, .p0inv = 0xfad08723},
	{.pub_key_n0 = 0x72c652b1, .n0inv = 0x46a949af, .p0inv = 0xb956b651},
	{.pub_key_n0 = 0xe7f34ed, .n0inv = 0x38c0b71b, .p0inv = 0xc73f48e5}
};

static uint32_t get_inv_mod(const struct vb2_public_key *key, int inv_type)
{
	for (size_t i = 0; i < ARRAY_SIZE(INVMOD); i++) {
		if (INVMOD[i].pub_key_n0 == key->n[0]) {
			if (inv_type < 0)
				return INVMOD[i].n0inv;
			else
				return INVMOD[i].p0inv;
		}
	}
	VB2_DEBUG("Could not find the key, where, n[0] = 0x%x\n", key->n[0]);
	return 0;
}

#endif
