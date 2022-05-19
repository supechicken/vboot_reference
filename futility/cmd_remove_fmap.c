/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "fmap.h"
#include "futility.h"

static const char usage[] = "\n"
	"Usage:  " MYNAME " %s [OPTIONS] FILE AREA [AREA ...]\n"
	"\n"
	"Remove specific FMAP area from the FMAP.\n"
	"IMPORTANT: This does not remove actual area, but only its FMAP entry\n"
	"\n"
	"Options:\n"
	"  -o OUTFILE     Write the result to this file, instead of modifying\n"
	"                   the input file. This is safer, since there are no\n"
	"                   safeguards against doing something stupid.\n"
	"\n"
	"Example:\n"
	"\n"
	"  This will the RO_VPD area, and scramble VBLOCK_B:\n"
	"\n"
	"  " MYNAME " %s bios.bin RO_VPD VBLOCK_B\n"
	"\n";

static void print_help(int argc, char *argv[])
{
	printf(usage, argv[0], argv[0]);
}

enum {
	OPT_HELP = 1000,
};
static const struct option long_opts[] = {
	/* name    hasarg *flag  val */
	{"help",        0, NULL, OPT_HELP},
	{NULL,          0, NULL, 0},
};
static const char *short_opts = ":o:";

static int remove_fmap_area(void *ptr, size_t size, FmapHeader *fmap,
			    const char *area_name)
{
	FmapAreaHeader *const first_ah =
		(FmapAreaHeader *)((void *)fmap + sizeof(FmapHeader));
	FmapAreaHeader *ah = NULL;

	if (!fmap_find_by_name(ptr, size, fmap, area_name, &ah))
		return 1;

	int area_number =
		((uintptr_t)ah - (uintptr_t)first_ah) / sizeof(FmapAreaHeader);

	VB2_DEBUG("Removing FMAP area %s at %d\n", area_name, area_number);

	void *dst = (uint8_t *)first_ah + area_number * sizeof(FmapAreaHeader);
	void *src = (uint8_t *)first_ah +
		    (area_number + 1) * sizeof(FmapAreaHeader);
	ssize_t src_size =
		(fmap->fmap_nareas - 1 - area_number) * sizeof(FmapAreaHeader);
	if (size > 0)
		memmove(dst, src, src_size);

	fmap->fmap_nareas -= 1;

	return 0;
}

static int do_remove_fmap(int argc, char *argv[])
{
	char *infile = 0;
	char *outfile = 0;
	uint8_t *buf;
	uint32_t len;
	FmapHeader *fmap;
	int errorcnt = 0;
	int fd, i;

	opterr = 0;
	while ((i = getopt_long(argc, argv, short_opts, long_opts, 0)) != -1) {
		switch (i) {
		case 'o':
			outfile = optarg;
			break;
		case OPT_HELP:
			print_help(argc, argv);
			return !!errorcnt;
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
		default:
			FATAL("Unrecognized getopt output: %d\n", i);
		}
	}

	if (errorcnt) {
		print_help(argc, argv);
		return 1;
	}

	if (argc - optind < 2) {
		fprintf(stderr, "You must specify an input file"
				" and at least one AREA argument\n");
		print_help(argc, argv);
		return 1;
	}

	infile = argv[optind++];

	if (outfile)
		futil_copy_file_or_die(infile, outfile);
	else
		outfile = infile;

	errorcnt |= futil_open_and_map_file(outfile, &fd, FILE_RW, &buf, &len);

	if (errorcnt)
		goto done;

	fmap = fmap_find(buf, len);
	if (!fmap) {
		fprintf(stderr, "Can't find an FMAP in %s\n", infile);
		errorcnt++;
		goto done;
	}

	for (i = optind; i < argc; i++) {
		const char *area = argv[i];

		if (remove_fmap_area(buf, len, fmap, area)) {
			errorcnt++;
			break;
		}
	}

done:
	errorcnt |= futil_unmap_and_close_file(fd, FILE_RW, buf, len);
	return !!errorcnt;
}

DECLARE_FUTIL_COMMAND(remove_fmap, do_remove_fmap, VBOOT_VERSION_ALL,
		      "Replace the contents of specified FMAP areas");
