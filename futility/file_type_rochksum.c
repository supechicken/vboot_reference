/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Some instances of the Chrome OS embedded controller firmware can't do a
 * normal software sync handshake at boot, but will verify their own RW images
 * instead. This is typically done by putting a struct vb2_packed_key in the RO
 * image and a corresponding struct vb21_signature in the RW image.
 *
 * This file provides the basic implementation for that approach.
 */

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "2common.h"
#include "2rsa.h"
#include "2sha.h"
#include "2sysincludes.h"
#include "file_type.h"
#include "fmap.h"
#include "futility.h"
#include "futility_options.h"
#include "host_common.h"
#include "host_common21.h"
#include "host_key21.h"
#include "host_misc.h"
#include "host_signature21.h"
#include "util_misc.h"

static void show_chksum(const char *fname, const struct vb2_hash *hash)
{
	int i = 0;

	if (fname)
		printf("Name:             %s\n", fname);

	printf(" Hash    ");
	for (i = 0; i < VB2_SHA256_DIGEST_SIZE; i++)
		printf("%x", hash->sha256[i]);

	printf("\n");
}

int ft_show_rochksum(const char *fname)
{
	uint32_t data_size = 0, hash_size = VB2_SHA256_DIGEST_SIZE;
	uint32_t total_data_size = 0;
	uint8_t *data;
	FmapHeader *fmap;
	int fd = -1;
	uint8_t *buf;
	uint32_t len;
	int i = 0;
	int rv = 1;
	const struct vb2_hash *hash;
	struct vb2_hash calc_hash;

	if (futil_open_and_map_file(fname, &fd, FILE_RO, &buf, &len))
		return 1;

	VB2_DEBUG("name %s len 0x%08x (%d)\n", fname, len, len);

	/* Am I just looking at a chksum file? */
	if (len == VB2_SHA256_DIGEST_SIZE) {
		hash = (const struct vb2_hash *)buf;

		show_chksum(fname, hash);
		if (!show_option.fv) {
			printf("No data available to verify\n");
			rv = show_option.strict;
			goto done;
		}
		data = show_option.fv;
		data_size = show_option.fv_size;
		total_data_size = show_option.fv_size;
	} else {
		fmap = fmap_find(buf, len);
		if (fmap) {
			/* This looks like a full image. */
			FmapAreaHeader *fmaparea;

			VB2_DEBUG("Found an FMAP!\n");

			hash = (const struct vb2_hash *)fmap_find_by_name(
				buf, len, fmap, "RO_CHECKSUM", &fmaparea);
			if (!hash) {
				VB2_DEBUG("No RO_CHECKSUM in FMAP.\n");
				goto done;
			}

			hash_size = fmaparea->area_size;

			VB2_DEBUG("Looking for checksum at %#tx (%#x)\n", (uint8_t *)hash - buf,
				  hash_size);

			vb2_hash_calculate(false, buf, len, VB2_HASH_SHA256, &calc_hash);

			if (memcmp(hash, &calc_hash, VB2_SHA256_DIGEST_SIZE)) {
				VB2_DEBUG("Invalid Hash found. Calculated:\n");
				show_chksum(fname, &calc_hash);
				VB2_DEBUG("Found:\n");
				show_chksum(fname, hash);
				goto done;
			} else {
				VB2_DEBUG("Valid hash Found:\n");
				show_chksum(fname, hash);
			}

			data = fmap_find_by_name(buf, len, fmap, "WP_RO", &fmaparea);

			total_data_size = fmaparea->area_size - hash_size;

			if (!data) {
				VB2_DEBUG("No WP_RO in FMAP.\n");
				goto done;
			}
		} else {
			/* Or maybe this is just the RO portion, that does not
			 * contain a FMAP. */
			if (show_option.sig_size)
				hash_size = show_option.sig_size;

			VB2_DEBUG("Looking for checksum at %#x\n", len - hash_size);

			if (len < hash_size) {
				VB2_DEBUG("File is too small\n");
				goto done;
			}

			hash = (const struct vb2_hash *)(buf + len - hash_size);

			vb2_hash_calculate(false, buf, len, VB2_HASH_SHA256, &calc_hash);

			if (memcmp(hash, &calc_hash, VB2_SHA256_DIGEST_SIZE)) {
				VB2_DEBUG("Invalid Hash found. Calculated:\n");
				show_chksum(fname, &calc_hash);
				VB2_DEBUG("Found:\n");
				show_chksum(fname, hash);

				goto done;
			} else {
				VB2_DEBUG("Valid hash Found:\n");
				show_chksum(fname, hash);
				data = buf;
				total_data_size = len - hash_size;
			}
		}
	}

	if (hash_size > total_data_size) {
		VB2_DEBUG("Invalid hash data_size: bigger than total area size.\n");
		goto done;
	}

	/* Check that the rest of region is padded with 0xff. */
	VB2_DEBUG("%s:, data_size=%x, total_data_size=%x\n", __func__, data_size,
		  total_data_size);
	for (i = data_size; i < total_data_size; i++) {
		if (data[i] != 0xff) {
			ERROR("Padding verification failed\n");
			goto done;
		}
	}

	printf("Hash verification succeeded.\n");
	rv = 0;
done:
	futil_unmap_and_close_file(fd, FILE_RO, buf, len);
	return rv;
}

int ft_sign_rochksum(const char *fname)
{
	uint8_t *data; /* data to be signed */
	uint32_t r, data_size, hash_size = VB2_SHA256_DIGEST_SIZE;
	int retval = 1;
	FmapHeader *fmap = NULL;
	FmapAreaHeader *fmaparea;
	struct vb2_hash *old_hash = 0;
	uint8_t *buf = NULL;
	uint32_t len;
	int fd = -1;
	struct vb2_hash *calc_hash;

	if (futil_open_and_map_file(fname, &fd, FILE_MODE_SIGN(sign_option), &buf, &len))
		return 1;

	data = buf;
	data_size = len;

	VB2_DEBUG("name %s len  0x%08x (%d)\n", fname, len, len);

	calc_hash = calloc(1, VB2_SHA256_DIGEST_SIZE);

	/* If we don't have a distinct OUTFILE, look for an existing sig */
	if (sign_option.inout_file_count < 2) {
		VB2_DEBUG("(sign_option.inout_file_ count < 2)");
		fmap = fmap_find(data, len);

		if (fmap) {
			/* This looks like a full image. */
			VB2_DEBUG("Found an FMAP!\n");

			old_hash = (struct vb2_hash *)fmap_find_by_name(
				buf, len, fmap, "RO_CHECKSUM", &fmaparea);
			if (!old_hash) {
				VB2_DEBUG("No RO_CHEKCSUM in FMAP.\n");
				goto done;
			}

			hash_size = fmaparea->area_size;

			VB2_DEBUG("Looking for checksum at %#tx (%#x)\n",
				  (uint8_t *)old_hash - buf, hash_size);

			data = fmap_find_by_name(buf, len, fmap, "WP_RO", &fmaparea);
			if (!data) {
				VB2_DEBUG("No WP_RO in FMAP.\n");
				goto done;
			}
		} else {
			/* Or maybe this is just the RO portion, that does not
			 * contain a FMAP. */
			if (sign_option.sig_size)
				hash_size = sign_option.sig_size;

			VB2_DEBUG("Looking for old checksum at %#x\n", len - hash_size);

			if (len < hash_size) {
				ERROR("File is too small\n");
				goto done;
			}

			/* Take a look */
			old_hash = (struct vb2_hash *)(buf + len - hash_size);
		}

		vb2_hash_calculate(false, buf, len, VB2_HASH_SHA256, calc_hash);

		if (memcmp(old_hash, calc_hash, VB2_SHA256_DIGEST_SIZE)) {

			VB2_DEBUG("Invalid Hash found. Calculated:\n");
			show_chksum(fname, calc_hash);
			VB2_DEBUG("Found:\n");
			show_chksum(fname, old_hash);

			ERROR("Can't find a valid hash\n");
			goto done;
		}

		/* Use the same extent again */
		data_size = VB2_SHA256_DIGEST_SIZE;

		VB2_DEBUG("Found hash: data_size is %#x (%d)\n", data_size, data_size);
	}

	/* Unless overridden */
	if (sign_option.data_size)
		data_size = sign_option.data_size;

	/* calculate the checksum */
	vb2_hash_calculate(false, buf, len, VB2_HASH_SHA256, calc_hash);

	if (sign_option.inout_file_count < 2) {
		/* Overwrite the old checksum */
		VB2_DEBUG("/* Overwrite the old checksum */");
		if (hash_size < VB2_SHA256_DIGEST_SIZE) {
			ERROR("New hash is too large (%d > %d)\n", VB2_SHA256_DIGEST_SIZE,
			      hash_size);
			goto done;
		}
		VB2_DEBUG("Replacing old checksum with new one\n");
		memset(old_hash, 0xff, hash_size);
		memcpy(old_hash, calc_hash, VB2_SHA256_DIGEST_SIZE);
		if (fmap && sign_option.ecrw_out) {
			VB2_DEBUG("Writing %s (size=%d)\n", sign_option.ecrw_out,
				  fmaparea->area_size);
			if (vb2_write_file(sign_option.ecrw_out, data, fmaparea->area_size))
				goto done;
		}
	} else {
		/* Write the hash to a new file */
		VB2_DEBUG(" Write the hash to a new file: %s, size=%ld\n", sign_option.outfile,
			  sizeof(calc_hash->sha256));

		show_chksum(fname, calc_hash);
		r = vb2_write_file(sign_option.outfile, calc_hash->sha256,
				   VB2_SHA256_DIGEST_SIZE);

		if (r) {
			ERROR("Unable to write checksum (error 0x%08x)\n", r);
			goto done;
		}
	}

	/* Finally */
	retval = 0;

done:
	futil_unmap_and_close_file(fd, FILE_MODE_SIGN(sign_option), buf, len);
	free(calc_hash);

	return retval;
}

enum futil_file_type ft_recognize_rochksum(uint8_t *buf, uint32_t len)
{
	const struct vb2_hash *hash = NULL;
	uint32_t hash_size;
	struct vb2_hash calc_hash;

	FmapHeader *fmap = fmap_find(buf, len);
	if (fmap) {
		/* This looks like a full image. */
		FmapAreaHeader *fmaparea;

		hash = (const struct vb2_hash *)fmap_find_by_name(buf, len, fmap, "RO_CHECKSUM",
								  &fmaparea);

		if (!hash)
			return FILE_TYPE_UNKNOWN;

		hash_size = fmaparea->area_size;
	} else {
		/* RO-only image */
		hash = (const struct vb2_hash *)(buf + len - VB2_SHA256_DIGEST_SIZE);
		hash_size = VB2_SHA256_DIGEST_SIZE;

		if (len < hash_size)
			return FILE_TYPE_UNKNOWN;
	}

	/* confirm that has is the correct size before checking */
	if (hash_size == VB2_SHA256_DIGEST_SIZE)
	{
		VB2_DEBUG("Calculating hash, len=%d\n", len);
		/* calculate the checksum */
		vb2_hash_calculate(false, buf, len - VB2_SHA256_DIGEST_SIZE,
			VB2_HASH_SHA256, &calc_hash);

		if (memcmp(hash, &calc_hash, VB2_SHA256_DIGEST_SIZE)) {

			VB2_DEBUG("Can't find a valid hash\n");
			return FILE_TYPE_UNKNOWN;

		} else {

			return FILE_TYPE_ROCHKSUM;
		}
	}


	return FILE_TYPE_UNKNOWN;
}
