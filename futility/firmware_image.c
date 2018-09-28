/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Utilities for manipulating an FMAP based firmware image.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "firmware_image.h"
#include "host_misc.h"

/*
 * Finds a firmware section by given name in the firmware image.
 * If successful, returns zero and section argument contains the address and
 * size of the section; otherwise failure.
 */
int firmware_find_section(const struct firmware_image *image,
			  struct firmware_section *section,
			  const char *section_name)
{
	FmapAreaHeader *fah = NULL;
	uint8_t *ptr;

	section->data = NULL;
	section->size = 0;
	ptr = fmap_find_by_name(
			image->data, image->size, image->fmap_header,
			section_name, &fah);
	if (!ptr)
		return -1;
	section->data = (uint8_t *)ptr;
	section->size = fah->area_size;
	return 0;
}

/* Returns 1 if given FMAP section exists in the firmware image, otherwise 0. */
int firmware_has_section(const struct firmware_image *image,
			 const char *section_name)
{
	struct firmware_section section;
	firmware_find_section(image, &section, section_name);
	return section.data != NULL;
}

/*
 * Preserves (copies) the given section (by name) from image_from to image_to.
 * The offset may be different, and the section data will be directly copied.
 * If the section does not exist on either images, return as failure.
 * If the source section is larger, contents on destination be truncated.
 * If the source section is smaller, the remaining area is not modified.
 * Returns 0 if success, non-zero if error.
 */
int firmware_preserve_section(const struct firmware_image *image_from,
			      struct firmware_image *image_to,
			      const char *section_name)
{
	struct firmware_section from, to;

	firmware_find_section(image_from, &from, section_name);
	firmware_find_section(image_to, &to, section_name);
	if (!from.data || !to.data) {
		DEBUG("Cannot find section %s: from=%p, to=%p", section_name,
		      from.data, to.data);
		return -1;
	}
	if (from.size > to.size) {
		printf("WARNING: %s: Section %s is truncated after updated.\n",
		       __FUNCTION__, section_name);
	}
	/* Use memmove in case if we need to deal with sections that overlap. */
	memmove(to.data, from.data, Min(from.size, to.size));
	return 0;
}

/*
 * Checks if the section is cleared (with given pattern).
 * If the section contains different data, return 0; otherwise non-zero.
 */
int firmware_section_is_cleared(const struct firmware_section *section,
				uint8_t pattern)
{
	uint32_t i;
	for (i = 0; i < section->size; i++)
		if (section->data[i] != pattern)
			return 0;
	return 1;
}

/*
 * Gets the firmware version from an FMAP section inside firmware image.
 * The section should only contain ASCIIZ string as firmware version.
 * If successful, returns a newly allocated version string (caller must free
 * it); otherwise NULL.
 */
char *firmware_get_version(struct firmware_image *image,
			   const char *section_name)
{
	struct firmware_section fwid;
	firmware_find_section(image, &fwid, section_name);
	if (!fwid.size)
		return NULL;
	return strndup((const char*)fwid.data, fwid.size);
}

/*
 * Loads a firmware image from file.
 * Returns 0 on success, otherwise failure.
 */
int firmware_load_from_file(struct firmware_image *image, const char *file_name)
{
	const char *rw_name_a = FMAP_RW_FWID_A, *rw_name_b = FMAP_RW_FWID_B;

	if (vb2_read_file(file_name, &image->data, &image->size) != VB2_SUCCESS)
		return -1;

	assert(image->data);
	image->file_name = strdup(file_name);
	image->fmap_header = fmap_find(image->data, image->size);
	if (!image->fmap_header) {
		ERROR("Invalid image file (missing FMAP): %s", file_name);
		return -1;
	}

	if (!firmware_has_section(image, FMAP_RO_FRID)) {
		ERROR("Does not look like VBoot firmware image: %s", file_name);
		return -1;
	}
	image->ro_version = firmware_get_version(image, FMAP_RO_FRID);

	if (firmware_has_section(image, FMAP_RW_FWID)) {
		rw_name_a = rw_name_b = FMAP_RW_FWID;
	} else if (!firmware_has_section(image, FMAP_RW_FWID_A)) {
		ERROR("Unsupported VBoot firmware (no RW ID): %s", file_name);
	}
	image->rw_version_a = firmware_get_version(image, rw_name_a);
	image->rw_version_b = firmware_get_version(image, rw_name_b);
	return 0;
}


/*
 * Frees the allocated resource from a firmware image object.
 */
void firmware_unload(struct firmware_image *image)
{
	free(image->data);
	free(image->file_name);
	free(image->ro_version);
	free(image->rw_version_a);
	free(image->rw_version_b);
	memset(image, 0, sizeof(*image));
}
