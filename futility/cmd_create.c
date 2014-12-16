/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <getopt.h>
#include <stdio.h>
#include <unistd.h>

#include "2sysincludes.h"
#include "2common.h"
#include "2guid.h"
#include "2rsa.h"
#include "vb21_common.h"
#include "vb21_struct.h"

#include "host_common.h"
#include "host_key2.h"
#include "host_misc2.h"

#include "futility.h"


/* Command line options */
enum {
	OPT_OUTFILE = 1000,
	OPT_VERSION,
	OPT_DESC,
	OPT_GUID,
	OPT_HASH_ALG,
};

static char *infile;
static char *outfile;
static uint32_t opt_version = 1;
static char *opt_desc;
static struct vb2_guid opt_guid;
static int opt_hash_alg = -1;

static const struct option long_opts[] = {
	{"outfile",  1, 0, OPT_OUTFILE},
	{"version",  1, 0, OPT_VERSION},
	{"desc",     1, 0, OPT_DESC},
	{"guid",     1, 0, OPT_GUID},
	{"hash_alg", 1, 0, OPT_HASH_ALG},
	{NULL, 0, 0, 0}
};

static void print_help(const char *progname)
{
	int i;

	printf("\n"
"Usage:  " MYNAME " %s [options] <INFILE> [<OUTFILE>]\n"
"\n"
"This creates a vboot 2.1 key from an RSA key file."
"\n"
"Options:\n"
"\n"
"  --outfile <OUTFILE>         Another way to specify the output file\n"
"  --version <number>          Key version (for .keyb only)\n"
"  --desc <string>             Human-readable description\n"
"  --hash_alg <number>         Hashing algorithm to use\n",
	       progname);

	for (i = 0; i < kNumAlgorithms; i++) {
		printf("                                %d = (%s)\n",
		       i, algo_strings[i]);
	}
}

/* Pack a .keyb file into a .vbpubk, or a .pem into a .vbprivk */
static int vb21_create_key(void)
{
	int r;
	struct vb2_public_key *key;
	struct vb21_packed_key *pkey;

	r = vb21_public_key_read_keyb(&key, infile);
	if (VB2_SUCCESS == r) {

		if (opt_desc) {
			r = vb21_public_key_set_desc(key, opt_desc);
			if (r) {
				fprintf(stderr, "Unable to set desc: r=%d\n", r);
				vb21_public_key_free(key);
				return r;
			}
		}

		key->hash_alg = opt_hash_alg;
		key->guid = &opt_guid;
		key->version = opt_version;

		r = vb21_public_key_pack(&pkey, key);
		if (r) {
			fprintf(stderr, "Unable to pack key: r=%d\n", r);
			goto pub_out;
		}

		r = vb2_write_object(outfile, pkey);
		if (r) {
			fprintf(stderr, "Unable to write keyfile: r=%d\n", r);
			goto pub_out;
		}

		/* seems to have worked */
		printf("Created public key %s\n", outfile);
		r = 0;
	pub_out:
		free(pkey);
		vb21_public_key_free(key);
		return r;
	}

// 	if (VB2_SUCCESS == vb21_private_key_read_pem(&key, infile)) {
// 		/* Private key */
// 		return 0;
// 	}

	fprintf(stderr, "Nuts: r=%d\n", r);
	return 1;
}

static int do_create(int argc, char *argv[])
{
	int errorcnt = 0;
	char *e;
	int i;

	while ((i = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
		switch (i) {
		case OPT_OUTFILE:
			outfile = optarg;
			break;

		case OPT_VERSION:
			opt_version = strtoul(optarg, &e, 0);
			if (!*optarg || (e && *e)) {
				fprintf(stderr,
					"invalid version \"%s\"\n", optarg);
				errorcnt = 1;
			}
			break;

		case OPT_DESC:
			opt_desc = optarg;
			break;

		case OPT_GUID:
			if (VB2_SUCCESS != vb2_str_to_guid(optarg,
							   &opt_guid)) {
				fprintf(stderr, "invalid guid \"%s\"\n",
					optarg);
				errorcnt = 1;
			}
			break;

		case OPT_HASH_ALG:
			opt_hash_alg = strtoul(optarg, &e, 0);
			if (!*optarg || (e && *e)) {
				fprintf(stderr,
					"invalid hash_alg \"%s\"\n", optarg);
				errorcnt = 1;
			}
			break;

		case '?':
			if (optopt)
				fprintf(stderr, "Unrecognized option: -%c\n",
					optopt);
			else
				fprintf(stderr, "Unrecognized option\n");
			errorcnt++;
			break;
		case ':':
			fprintf(stderr, "Missing argument to -%c\n", optopt);
			errorcnt++;
			break;
		case 0:				/* handled option */
			break;
		default:
			DIE;
		}
	}

	/* If we don't have an input file already, we need one */
	if (!infile) {
		if (argc - optind <= 0) {
			fprintf(stderr, "ERROR: missing input filename\n");
			errorcnt++;
		} else {
			infile = argv[optind++];
		}
	}

	/* We need an output file too (for now) */
	/* TODO: Name the output after the input? */
	if (!outfile) {
		if (argc - optind <= 0) {
			fprintf(stderr, "ERROR: missing output filename\n");
			errorcnt++;
		} else {
			outfile = argv[optind++];
		}
	}

	if (errorcnt) {
		print_help(argv[0]);
		return 1;
	}

	/* Okay, do it */
	return !!vb21_create_key();
}

DECLARE_FUTIL_COMMAND(create, do_create,
		      VBOOT_VERSION_2_1,
		      "Create vb21 key from RSA file",
		      print_help);
