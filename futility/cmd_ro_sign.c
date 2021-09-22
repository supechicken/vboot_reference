/*
 * Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <openssl/bn.h>
#include <openssl/pem.h>
#include <stdbool.h>
#include <stdlib.h>

#include "fmap.h"
#include "futility.h"
#include "gsc_ro.h"
#include "host_key21.h"
#include "host_keyblock.h"
#include "host_signature.h"

/*
 * for testing purposes let's use
 * - tests/devkeys/kernel_subkey.vbprivk as the root private key
 * - tests/devkeys/kernel_subkey.vbpubk as the root public key
 *   used for signing of the platform public key
 * - tests/devkeys/firmware_data_key.vbprivk signing platform key
 * - tests/devkeys/firmware_data_key.vbpubk - public key used for signature
 *       verification
 *------------
 * Command to create the signed public key block in ~/tmp/packed:
 *
  ./build/futility/futility vbutil_keyblock --pack ~/tmp/packed \
       --datapubkey tests/devkeys/firmware_data_key.vbpubk	 \
       --signprivate tests/devkeys/kernel_subkey.vbprivk
 *------------
 * Command to fill RO_GSCVD area in a BIOS file with the area defined in FMAP,
 *   input BIOS file is ~/tmp/image-guybrush.serial.bin, signed BIOS file saved
 *   in ~/tmp/guybrush-signed:
 *
  ./build/futility/futility ro_sign --outfile ~/tmp/guybrush-signed \
     -R 818100:10000,f00000:100,f80000:2000,f8c000:1000,0x00804000:0x00000800 \
     -k ~/tmp/packed -p tests/devkeys/firmware_data_key.vbprivk	       \
     -r tests/devkeys/kernel_subkey.vbpubk ~/tmp/image-guybrush.serial.bin
 *------------
 * Command to validate a previously signed BIOS file. The hash is the sha256sum
 *  of tests/devkeys/kernel_subkey.vbpubk:
 *
  build/futility/futility ro_sign ~/tmp/guybrush-signed \
   36b9c5fa6f5d0432b9acbe8e2b7da2e602162a87a425575c6d7ba975a0440708
 */

/* Command line options processing support. */
enum no_short_opts {
	OPT_OUTFILE = 1000,
};

static const struct option long_opts[] = {
	/* name       hasarg *flag  val */
	{"outfile",       1, NULL, OPT_OUTFILE},
	{"ranges",        1, NULL, 'R'},
	{"root_pub_key",  1, NULL, 'r'},
	{"keyblock",      1, NULL, 'k'},
	{"platform_priv", 1, NULL, 'p'},
	{"help",          0, NULL, 'h'},
	{}
};

static const char *short_opts = "R:hk:p:r:";

static const char usage[] =
	"\n"
	"This utility creates an RO verification space in Chrome OS bios\n"
	"image and allows to validate a previously prepared image containing\n"
	"the RO verification space.\n\n"
	"Usage: ro_sign PARAMS BIOS_FILE [<root key hash>]\n\n\n"
	"Creation of RO Verification space:\n\n"
	"Required PARAMS:\n"
	"  -R|--ranges        STRING        Comma separated colon delimited\n"
	"                                     hex tuples <offset>:<size>, the\n"
	"                                     areas of the RO covered by the\n"
	"                                     signature\n"
	"  -r|--root_pub_key  FILE.vbpubk   The main public key, used to\n"
	"                                     verify platform key\n"
	"  -k|--keyblock      FILE.keyblock Signed platform public key used\n"
	"                                     for run time RO verifcation\n"
	"  -p|--platform_priv FILE.vbprivk  Private platform key used for\n"
	"                                     signing RO verification data\n"
	"\n"
	"Optional PARAMS:\n"
	"  [--outfile]        OUTFILE       Output firmware image containing\n"
	"                                     RO verification information\n"
	"\n\n"
	"Validation of RO Verification space:\n\n"
	"   The only required parameter is BIOS_FILE, if optional\n"
	"   <root key hash> is given, it is compared to the hash\n"
	"   of the root key found in the input file.\n"
	"\n\n"
	"  -h|--help                        Print this message\n"
	"\n";


/* Structure helping to keep track of the file mapped into memory. */
struct file_buf {
	uint32_t len;
	uint8_t *data;
	FmapAreaHeader *ro_gscvd;
};

/*
 * Max number of RO ranges to cover. 32 is more than enough, this must be kept
 * in sync with APRO_MAX_NUM_RANGES declaration in
 * common/ap_ro_integrity_check.c in the Cr50 tree.
 */
#define MAX_RANGES 32

/*
 * Container keeping track of the set of ranges to include in hash
 * calculation.
 */
struct ro_ranges {
	size_t range_count;
	struct ro_range ranges[MAX_RANGES];
};

/**
 * Read the BIOS file.
 *
 * Map the requested file into memory, find RO_GSCVD area in the file, and
 * cache the information in the passed in file_buf structure.
 *
 * @param file_name  name of the BIOS file
 * @param file_buf   pointer to the helper structure keeping information about
 *                   the file
 *
 * @return 0 on success 1 on failure.
 */
static int read_bios(const char *file_name, struct file_buf *file)
{
	int fd;
	int rv;

	fd = open(file_name, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "Can't open %s: %s\n", file_name,
			strerror(errno));
		return 1;
	}

	do {
		rv = 1;

		if (futil_map_file(fd, MAP_RW, &file->data, &file->len))
			break;

		if (!fmap_find_by_name(file->data, file->len, NULL, "RO_GSCVD",
				       &file->ro_gscvd)) {
			fprintf(stderr,
				"Could not find RO_GSCVD in the FMAP\n");
			break;
		}
		rv = 0;
	} while (false);

	return rv;
}

/**
 * Check if the passed in offset falls into the passed in FMAP area.
 */
static bool in_range(uint32_t offset, const FmapAreaHeader *ah)
{
	return (offset >= ah->area_offset) &&
	       (offset < (ah->area_offset + ah->area_size));
}

/**
 * Check if the passed in range fits into the passed in FMAP area.
 */
static bool range_fits(const struct ro_range *range, const FmapAreaHeader *ah)
{
	if (in_range(range->offset, ah) &&
	    in_range(range->offset + range->size, ah))
		return true;

	fprintf(stderr, "Range %x..+%x does not fit in %s\n", range->offset,
		range->size, ah->area_name);

	return false;
}

/**
 * Check if the passed in range overlaps with the area.
 *
 * @param range  pointer to the range to check
 * @param offset  offset of the area to check against
 * @param size  size of the area to check against
 *
 * @return true if range overlaps with the area, false otherwise.
 */
static bool range_overlaps(const struct ro_range *range, uint32_t offset,
			   size_t size)
{
	if (((range->offset + range->size) <= offset) ||
	    (offset + size) <= range->offset)
		return false;

	fprintf(stderr, "Range %x..+%x overlaps with %x..+%zx\n", range->offset,
		range->size, offset, size);

	return true;
}

/*
 * Check validity of the passed in ranges.
 *
 * All ranges must
 * - fit into the WP_RO FMAP area
 * - not overlap with the RO_GSCVD FMAP area
 * - not overlap with each other
 *
 * @param ranges - pointer to the container of ranges to check
 * @param file - pointer to the file layout descriptor
 *
 * @return zero on success, number of encountered errors on failure.
 */
static int verify_ranges(const struct ro_ranges *ranges,
			 const struct file_buf *file)
{
	size_t i;
	FmapAreaHeader *wp_ro;
	int errorcount;

	if (!fmap_find_by_name(file->data, file->len, NULL, "WP_RO", &wp_ro)) {
		fprintf(stderr, "Could not find WP_RO in the FMAP\n");
		return 1;
	}

	errorcount = 0;
	for (i = 0; i < ranges->range_count; i++) {
		size_t j;

		/* Must fit into WP_RO. */
		if (!range_fits(ranges->ranges + i, wp_ro))
			errorcount++;

		/* Must not overlap with RO_GSCVD. */
		if (range_overlaps(ranges->ranges + i,
				   file->ro_gscvd->area_offset,
				   file->ro_gscvd->area_size))
			errorcount++;

		/* The last range is nothing to compare against. */
		if (i == ranges->range_count - 1)
			break;

		/* Must not overlap with all following ranges. */
		for (j = i + 1; j < ranges->range_count; j++)
			if (range_overlaps(ranges->ranges + i,
					   ranges->ranges[j].offset,
					   ranges->ranges[j].size))
				errorcount++;
	}

	return errorcount;
}

/**
 * Parse range specification supplied by the user.
 *
 * The input is a string of the following format:
 * <hex base>:<hex size>[,<hex base>:<hex size>[,...]]
 *
 * @param input  user input, part of the command line
 * @param output  pointer to the ranges container
 *
 * @return zero on success, -1 on failure
 */
static int parse_ranges(const char *input, struct ro_ranges *output)
{
	char *cursor;
	char *delim;
	char *str = strdup(input);
	int rv = 0;

	if (!str) {
		fprintf(stderr, "Failed to allocate memory for "
				"ranges string copy!\n");
		return -1;
	}

	output->range_count = 0;
	cursor = str;
	do {
		char *colon;
		char *e;

		if (output->range_count >= ARRAY_SIZE(output->ranges)) {
			fprintf(stderr, "Too many ranges!\n");
			rv = -1;
			break;
		}

		delim = strchr(cursor, ',');
		if (delim)
			*delim = '\0';
		colon = strchr(cursor, ':');
		if (!colon) {
			rv = -1;
			break;
		}
		*colon = '\0';

		errno = 0;
		output->ranges[output->range_count].offset =
			strtol(cursor, &e, 16);
		if (errno || *e) {
			rv = -1;
			break;
		}

		output->ranges[output->range_count].size =
			strtol(colon + 1, &e, 16);
		if (errno || *e) {
			rv = -1;
			break;
		}

		output->range_count++;
		cursor = delim + 1;
		/* Iterate until there is no more commas. */
	} while (delim);

	free(str);
	if (rv)
		fprintf(stderr, "Misformatted ranges string\n");

	return rv;
}

/**
 * Calculate hash of the RO ranges.
 *
 * @param bios_file  pointer to the bios file layout descriptor
 * @param ranges  pointer to the container of ranges to include in hash
 *		  calculation
 * @param hash_alg  algorithm to use for hashing
 * @param digest  memory to copy the calculated hash to
 * @param digest_ size requested size of the digest, padded with zeros if the
 *	          SHA digest size is smaller than digest_size
 *
 * @return zero on success, -1 on failure.
 */
static int calculate_ranges_digest(const struct file_buf *bios_file,
				   const struct ro_ranges *ranges,
				   enum vb2_hash_algorithm hash_alg,
				   void *digest, size_t digest_size)
{
	struct vb2_digest_context dc;
	size_t i;

	/* Calculate the ranges digest. */
	if (vb2_digest_init(&dc, hash_alg) != VB2_SUCCESS) {
		fprintf(stderr, "%s: Failed to init digest!\n", __func__);
		return 1;
	}

	for (i = 0; i < ranges->range_count; i++) {
		if (vb2_digest_extend(
			    &dc, bios_file->data + ranges->ranges[i].offset,
			    ranges->ranges[i].size) != VB2_SUCCESS) {
			fprintf(stderr, "%s: Failed to extend digest!\n",
				__func__);
			return -1;
		}
	}

	memset(digest, 0, digest_size);
	if (vb2_digest_finalize(&dc, digest, digest_size) != VB2_SUCCESS) {
		fprintf(stderr, "%s: Failed to finalize digest!\n", __func__);
		return -1;
	}

	return 0;
}

/**
 * Fill up GVD structure header.
 *
 * Most of the information is hardcoded, the only variable part is the digest
 * of the AP RO ranges included in the verification.
 *
 * @param bios_file  pointer to the bios file layout descriptor
 * @param ranges  pointer to the container of ranges to include in verification
 * @param gvd  pointer to the structure to fill.
 *
 * @return zero on success, non-zero on failure.
 */
static int fill_up_gvd(struct file_buf *bios_file, struct ro_ranges *ranges,
		       struct gsc_verification_data *gvd)
{
	const FmapHeader *fmh;

	memset(gvd, 0, sizeof(*gvd));

	gvd->gv_magic = GSC_VD_MAGIC;
	gvd->size =
		sizeof(*gvd) + sizeof(struct ro_range) * ranges->range_count;

	gvd->rollback_counter = GSC_VD_ROLLBACK_COUNTER;

	/* Guaranteed to succeed. */
	fmh = fmap_find(bios_file->data, bios_file->len);

	gvd->fmap_location = (uintptr_t)fmh - (uintptr_t)bios_file->data;
	gvd->range_count = ranges->range_count;

	gvd->hash_alg = VB2_HASH_SHA256;

	return calculate_ranges_digest(bios_file, ranges, gvd->hash_alg,
				       gvd->ranges_digest,
				       sizeof(gvd->ranges_digest));
}

/**
 * Sign GSC verification data.
 *
 * Concatenate the header and the ranges array in a flat memory block and
 * invoke the vb2 library function to produce the signature.
 *
 * @param gvd  pointer to the GVD header
 * @param ranges  pointer to the container of ranges to include in verification
 * @param privk   pointer to the private key to use for signing
 * @param sig   pointer to pointer to signature. Populated if signing succeeded.
 *	        The caller is supposed to free the memory allocated for the
 *	        signature.
 *
 * @return zero on success, non zero on failure.
 */
static int sign_gvd(struct gsc_verification_data *gvd, struct ro_ranges *ranges,
		    struct vb2_private_key *privk, struct vb2_signature **sig)
{
	uint8_t *buf;

	/* Concatenate GVD header with ranges. */
	buf = malloc(gvd->size);
	if (!buf) {
		fprintf(stderr, "%s: Failed to allocate %d bytes\n", __func__,
			gvd->size);
		return 1;
	}

	memcpy(buf, gvd, sizeof(*gvd));
	memcpy(buf + sizeof(*gvd), ranges->ranges,
	       ranges->range_count * sizeof(ranges->ranges[0]));

	*sig = vb2_calculate_signature(buf, gvd->size, privk);

	free(buf);

	return *sig == NULL;
}

/**
 * Fill RO_GSCVD FMAP area.
 *
 * All trust chain components have been verified, AP RO sections digest
 * calculated, and GVD signature created; put it all together in the dedicated
 * FMAP area.
 *
 * @param bios_file  pointer to the bios file layout descriptor
 * @param gvd  pointer to the GVD header
 * @param ranges  pointer to the container of ranges included in verification
 * @param signature  pointer to the signature container
 * @param keyblock  pointer to the keyblock container
 * @param root_pubk  pointer to the root pubk container
 *
 * @return zero on success, -1 on failure
 */
static int fill_gvd_area(struct file_buf *bios_file,
			 struct gsc_verification_data *gvd,
			 struct ro_ranges *ranges,
			 struct vb2_signature *signature,
			 struct vb2_keyblock *keyblock,
			 struct vb2_packed_key *root_pubk)
{
	size_t total;
	uint8_t *cursor;
	size_t copy_size;

	/* How much room is needed for the whole thing? */
	total = gvd->size + signature->sig_offset + signature->sig_size +
		keyblock->keyblock_size + root_pubk->key_offset +
		root_pubk->key_size;

	if (total > bios_file->ro_gscvd->area_size) {
		fprintf(stderr, "%s: GVD section does not fit, %zd > %d\n",
			__func__, total, bios_file->ro_gscvd->area_size);
		return -1;
	}

	cursor = bios_file->data + bios_file->ro_gscvd->area_offset;

	/* Copy GSC verification data */
	memcpy(cursor, gvd, sizeof(*gvd));
	cursor += sizeof(*gvd);
	copy_size = ranges->range_count * sizeof(ranges->ranges[0]);
	memcpy(cursor, ranges->ranges, copy_size);
	cursor += copy_size;

	/* Copying signature */
	copy_size = signature->sig_offset + signature->sig_size;
	memcpy(cursor, signature, copy_size);
	cursor += copy_size;

	/* Keyblock, size includes everything. */
	memcpy(cursor, keyblock, keyblock->keyblock_size);
	cursor += keyblock->keyblock_size;

	/* Root pubk, size does not include data. */
	memcpy(cursor, root_pubk, root_pubk->key_offset + root_pubk->key_size);

	return 0;
}

/**
 * Initialize a work buffer structure.
 *
 * Embedded vboot reference code does not use malloc/free, it uses the so
 * called work buffer structure to provide a poor man's memory management
 * tool. This program uses some of the embedded library functions, let's
 * implement work buffer support to keep the embedded code happy.
 *
 * @param wb  pointer to the workubffer structure to initialize
 * @param size  size of the buffer to allocate
 *
 * @return pointer to the allocated buffer on succes, NULL on failure.
 */
static void *init_wb(struct vb2_workbuf *wb, size_t size)
{
	wb->buf = malloc(size);
	if (wb->buf)
		wb->size = size;
	else
		fprintf(stderr, "%s: failed to allocate workblock of %zd\n",
			__func__, size);

	return wb->buf;
}

/**
 * Validate that platform key keyblock was signed by the root key
 *
 * This function performs the same step the GSC is supposed to perform:
 * validate the platform key keyblock signature using the root public key.
 *
 * @param root_pubk  pointer to the root public key container
 * @param kblock  pointer to the platform public key keyblock
 *
 * @return 0 on success, -1 on failure
 */
static int validate_pubk_signature(const struct vb2_packed_key *root_pubk,
				   struct vb2_keyblock *kblock)
{
	struct vb2_public_key pubk;
	struct vb2_workbuf wb;
	uint32_t kbsize;
	int rv;
	void *buf;

	if (vb2_unpack_key_buffer(&pubk, (const uint8_t *)root_pubk,
				  sizeof(*root_pubk) + root_pubk->key_size) !=
	    VB2_SUCCESS) {
		fprintf(stderr, "%s: failed to unpack public key\n", __func__);
		return -1;
	}

	/* Let's create an ample sized work buffer. */
	buf = init_wb(&wb, 8192);
	if (!buf)
		return -1;

	rv = -1;
	do {
		void *work;

		kbsize = kblock->keyblock_size;
		work = vb2_workbuf_alloc(&wb, kbsize);
		if (!work) {
			fprintf(stderr,
				"%s: failed to allocate workblock space %d\n",
				__func__, kbsize);
			break;
		}

		memcpy(work, kblock, kbsize);

		if (vb2_verify_keyblock(work, kbsize, &pubk, &wb) !=
		    VB2_SUCCESS) {
			fprintf(stderr, "%s: root and keyblock mismatch\n",
				__func__);
			break;
		}

		rv = 0;
	} while (false);

	free(buf);

	return rv;
}

/**
 * Validate that private and public parts of the platform key match.
 *
 * This is a fairly routine validation, the N components of the private and
 * public RSA keys are compared.
 *
 * @param keyblock  pointer to the keyblock containing the public key
 * @param plat_privk  pointer to the matching private key
 *
 * @return 0 on success, nonzero on failure
 */
static int validate_privk(struct vb2_keyblock *kblock,
			  struct vb2_private_key *plat_privk)
{
	const BIGNUM *privn;
	BIGNUM *pubn;
	struct vb2_public_key pubk;
	int rv;

	privn = pubn = NULL;

	RSA_get0_key(plat_privk->rsa_private_key, &privn, NULL, NULL);

	if (vb2_unpack_key_buffer(&pubk, (const uint8_t *)&kblock->data_key,
				  kblock->data_key.key_offset +
					  kblock->data_key.key_size) !=
	    VB2_SUCCESS) {
		fprintf(stderr, "Failed to unpack public key\n");
		return -1;
	}

	pubn = BN_new();
	pubn = BN_lebin2bn((uint8_t *)pubk.n, vb2_rsa_sig_size(pubk.sig_alg),
			   pubn);
	rv = BN_cmp(pubn, privn);
	if (rv)
		fprintf(stderr, "Public/private key N mismatch!\n");

	BN_free(pubn);
	return rv;
}

/**
 * Copy ranges from BIOS file into ro_ranges container
 *
 * While copying the ranges verify that they do not overlap.
 *
 * @param bios_file  pointer to the bios file layout descriptor
 * @param gvd  pointer to the GVD header followed by the ranges
 * @param ranges  pointer to the ranges container to copy ranges to
 *
 * @return 0 on successful copy nonzero on errors.
 */
static int copy_ranges(const struct file_buf *bios_file,
		       const struct gsc_verification_data *gvd,
		       struct ro_ranges *ranges)
{
	ranges->range_count = gvd->range_count;
	memcpy(ranges->ranges, gvd->ranges,
	       sizeof(ranges->ranges[0]) * ranges->range_count);

	return verify_ranges(ranges, bios_file);
}

/**
 * Basic validation of GVD included in a BIOS file
 *
 * This is not a cryptographic verification, just a check that the structure
 * makes sense and the expected values are found in certain fields.
 *
 * @param gvd  pointer to the GVD header followed by the ranges
 * @param bios_file  pointer to the bios file layout descriptor
 *
 * @rerurn zero on success, -1 on failure.
 */
static int validate_gvd(const struct gsc_verification_data *gvd,
			const struct file_buf *bios_file)
{
	const FmapHeader *fmh;

	if (gvd->gv_magic != GSC_VD_MAGIC) {
		fprintf(stderr, "Incorrect gscvd magic %x\n", gvd->gv_magic);
		return -1;
	}

	if (!gvd->range_count || (gvd->range_count > MAX_RANGES)) {
		fprintf(stderr, "Incorrect gscvd range count %d\n",
			gvd->range_count);
		return -1;
	}

	/* Guaranteed to succeed. */
	fmh = fmap_find(bios_file->data, bios_file->len);

	if (gvd->fmap_location !=
	    ((uintptr_t)fmh - (uintptr_t)bios_file->data)) {
		fprintf(stderr, "Incorrect gscvd fmap offset %x\n",
			gvd->fmap_location);
		return -1;
	}

	return 0;
}

/**
 * Validate GVD signature.
 *
 * Given the entire GVD space (header plus ranges array), the signature and
 * the public key, verify that the signature matches.
 *
 * @param gvd  pointer to gsc_verification_data followed by the ranges array
 * @param gvd_signature  pointer to the vb2 signature container
 * @param packedk  pointer to the keyblock containing the public key
 *
 * @return zero on success, non-zero on failure
 */
static int validate_gvd_signature(const struct gsc_verification_data *gvd,
				  struct vb2_signature *gvd_signature,
				  const struct vb2_packed_key *packedk)
{
	struct vb2_workbuf wb;
	void *buf;
	int rv;
	struct vb2_public_key pubk;

	/* Extract public key from the public key keyblock. */
	if (vb2_unpack_key_buffer(&pubk, (const uint8_t *)packedk,
				  sizeof(*packedk) + packedk->key_size) !=
	    VB2_SUCCESS) {
		fprintf(stderr, "%s: failed to unpack public key\n", __func__);
		return -1;
	}

	/* Let's create an ample sized work buffer. */
	buf = init_wb(&wb, 8192);
	if (!buf)
		return -1;

	rv = vb2_verify_data((const uint8_t *)gvd, gvd->size, gvd_signature,
			     &pubk, &wb);

	free(buf);
	return rv;
}

/**
 * Get a hex byte from the input string.
 *
 * Convert the first two ASCII characters of the input string into an 8 bit
 * integer. Update the input to point to the next two character in the string.
 *
 * @param input  pointer to pointer to the ASCIIZ string to process
 *
 * @return value in 0..255 range on success, -1 on failure.
 */
static int get_hex_byte(const char **input)
{
	char space[3] = {};
	int result;
	char *e;

	/*
	 * Create an ASCIIZ string from the two first characters of the
	 * input.
	 */
	strncpy(space, *input, 2);
	if (!space[0] || !space[1]) {
		fprintf(stderr, "Key hash value too short\n");
		return -1;
	}

	*input += 2;
	result = strtoul(space, &e, 16);
	if (*e) {
		fprintf(stderr, "Invalid hex character in key hash\n");
		return -1; /* Invalid character in the string. */
	}

	return result;
}

/**
 * Given a data blob and a digest verify that the digest is sha256 of the data
 *
 * @return zero on success, non zero on failure.
 */
static int validate_sha256_digest(const void *data, size_t size,
				  const void *expected_digest)
{
	uint8_t digest[SHA256_DIGEST_LENGTH];
	SHA256_CTX context;
	int rv;

	SHA256_Init(&context);
	SHA256_Update(&context, data, size);
	SHA256_Final(digest, &context);

	rv = memcmp(digest, expected_digest, sizeof(digest));

	if (rv)
		fprintf(stderr, "Sha256 mismatch\n");

	return rv;
}

/*
 * Validate GVD of the passed in BIOS file and possibly the root key hash
 *
 * The input parameters are the subset of the command line, the first argv
 * string is the BIOS file name, the second string, if present, is the hash of
 * the root public key included in the RO_GSCVD area of the BIOS file.
 *
 * @return zero on success, -1 on failure.
 */
static int validate_gscvd(int argc, char *argv[])
{
	struct file_buf bios_file;
	int rv;
	struct ro_ranges ranges;
	const struct gsc_verification_data *gvd;
	const char *file_name;
	uint8_t digest[sizeof(gvd->ranges_digest)];
	uint8_t root_key_digest_input[SHA256_DIGEST_LENGTH];

	/* Guaranteed to be available. */
	file_name = argv[0];

	if (argc > 1) {
		/* Root key sha256 digest is passed as the second parameter. */
		size_t i;
		const char *input = argv[1];

		for (i = 0; i < sizeof(root_key_digest_input); i++) {
			int value;

			value = get_hex_byte(&input);
			if (value < 0)
				return -1;

			root_key_digest_input[i] = value;
		}

		if (*input) {
			fprintf(stderr, "Key hash value too long\n");
			return -1;
		}
	}

	do {
		const struct vb2_packed_key *root_pubk;
		struct vb2_keyblock *kblock;
		struct vb2_signature *gvd_signature;

		rv = -1; /* Speculative, will be cleared on success. */

		if (read_bios(file_name, &bios_file))
			break;

		/* Copy ranges from gscvd to local structure. */
		gvd = (struct gsc_verification_data *)(bios_file.data +
						       bios_file.ro_gscvd
							       ->area_offset);

		if (validate_gvd(gvd, &bios_file))
			break;

		if (copy_ranges(&bios_file, gvd, &ranges))
			break;

		if (calculate_ranges_digest(&bios_file, &ranges, gvd->hash_alg,
					    digest, sizeof(digest)))
			break;

		if (memcmp(digest, gvd->ranges_digest, sizeof(digest))) {
			fprintf(stderr, "Ranges digest mismatch\n");
			break;
		}

		/* Init pointers to the crypto structures. */
		gvd_signature =
			(struct vb2_signature *)((uintptr_t)gvd + gvd->size);
		kblock =
			(struct vb2_keyblock *)(gvd_signature->sig_size +
						(uintptr_t)(gvd_signature + 1));
		root_pubk = (struct vb2_packed_key *)((uintptr_t)kblock +
						      kblock->keyblock_size);

		if ((argc > 1) &&
		    validate_sha256_digest(root_pubk,
					   root_pubk->key_offset +
						   root_pubk->key_size,
					   root_key_digest_input))
			break;

		if (validate_pubk_signature(root_pubk, kblock))
			break;

		if (validate_gvd_signature(gvd, gvd_signature,
					   &kblock->data_key))
			break;

		rv = 0;
	} while (false);

	return rv;
}

/**
 * The main function of this futilty option.
 *
 * See the usage string for input details.
 *
 * @return zero on success, nonzero on failure.
 */
static int do_ro_sign(int argc, char *argv[])
{
	int i;
	int longindex;
	char *infile = NULL;
	char *outfile = NULL;
	char *work_file = NULL;
	struct ro_ranges ranges;
	int errorcount = 0;
	struct vb2_packed_key *root_pubk = NULL;
	struct vb2_keyblock *kblock = NULL;
	struct vb2_private_key *plat_privk = NULL;
	struct vb2_signature *gvd_signature = NULL;
	struct file_buf bios_file;
	int rv = 0;
	struct gsc_verification_data gvd;

	ranges.range_count = 0;

	while ((i = getopt_long(argc, argv, short_opts, long_opts,
				&longindex)) != -1) {
		switch (i) {
		case OPT_OUTFILE:
			outfile = optarg;
			break;
		case 'R':
			if (parse_ranges(optarg, &ranges)) {
				fprintf(stderr, "Error parsing ranges\n");
				/* Error message has been already printed. */
				errorcount++;
			}
			break;
		case 'r':
			root_pubk = vb2_read_packed_key(optarg);
			if (!root_pubk) {
				fprintf(stderr, "Error reading %s\n", optarg);
				errorcount++;
			}
			break;
		case 'k':
			kblock = vb2_read_keyblock(optarg);
			if (!kblock) {
				fprintf(stderr, "Error reading %s\n", optarg);
				errorcount++;
			}
			break;
		case 'p':
			plat_privk = vb2_read_private_key(optarg);
			if (!plat_privk) {
				fprintf(stderr, "Error reading %s\n", optarg);
				errorcount++;
			}
			break;
		case 'h':
			printf("%s", usage);
			return 0;
		case '?':
			if (optopt)
				fprintf(stderr, "Unrecognized option: -%c\n",
					optopt);
			else
				fprintf(stderr, "Unrecognized option: %s\n",
					argv[optind - 1]);
			errorcount++;
			break;
		case ':':
			fprintf(stderr, "Missing argument to -%c\n", optopt);
			errorcount++;
			break;
		case 0: /* handled option */
			break;
		default:
			FATAL("Unrecognized getopt output: %d\n", i);
		}
	}

	if ((optind == 1) && (argc > 1))
		/* This must be a validation request. */
		return validate_gscvd(argc - 1, argv + 1);

	if (optind != (argc - 1)) {
		fprintf(stderr, "Misformatted command line\n%s\n", usage);
		return 1;
	}

	if (errorcount || !ranges.range_count || !root_pubk || !kblock ||
	    !plat_privk) {
		/* Error message(s) should have been printed by now. */
		fprintf(stderr, "%s\n", usage);
		return 1;
	}

	infile = argv[optind];

	if (outfile) {
		futil_copy_file_or_die(infile, outfile);
		work_file = outfile;
	} else {
		work_file = infile;
	}

	do {
		rv = 1; /* Speculative, will be cleared on success. */

		if (validate_pubk_signature(root_pubk, kblock))
			break;

		if (validate_privk(kblock, plat_privk))
			break;

		if (read_bios(work_file, &bios_file))
			break;

		if (verify_ranges(&ranges, &bios_file))
			break;

		if (fill_up_gvd(&bios_file, &ranges, &gvd))
			break;

		if (sign_gvd(&gvd, &ranges, plat_privk, &gvd_signature))
			break;

		if (fill_gvd_area(&bios_file, &gvd, &ranges, gvd_signature,
				  kblock, root_pubk))
			break;

		rv = 0;
	} while (false);

	free(gvd_signature);
	free(root_pubk);
	free(kblock);
	vb2_private_key_free(plat_privk);

	/*
	 * Now flush the file. Note that the fd parameter is not used by the
	 * called function.
	 */
	rv |= futil_unmap_file(-1, true, bios_file.data, bios_file.len);

	return rv;
}

DECLARE_FUTIL_COMMAND(ro_sign, do_ro_sign, VBOOT_VERSION_2_1,
		      "Create RO verification structure");
