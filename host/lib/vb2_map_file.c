/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#if !defined(HAVE_MACOS) && !defined(__FreeBSD__) && !defined(__OpenBSD__)
#include <linux/fs.h>		/* For BLKGETSIZE64 */
#include <sys/sendfile.h>
#else
#include <copyfile.h>
#endif


#include "2common.h"
#include "vb2_map_file.h"
#include "../../futility/futility.h"

enum file_err open_file(const char *infile, int *fd, enum file_mode mode)
{
	if (mode == FILE_RW) {
		VB2_DEBUG("open RW %s\n", infile);
		*fd = open(infile, O_RDWR);
		if (*fd < 0) {
			ERROR("Can't open %s for writing: %s\n", infile,
			      strerror(errno));
			return FILE_ERR_OPEN;
		}
	} else {
		VB2_DEBUG("open RO %s\n", infile);
		*fd = open(infile, O_RDONLY);
		if (*fd < 0) {
			ERROR("Can't open %s for reading: %s\n", infile,
			      strerror(errno));
			return FILE_ERR_OPEN;
		}
	}
	return FILE_ERR_NONE;
}

enum file_err close_file(int fd)
{
	if (fd >= 0 && close(fd)) {
		ERROR("Closing ifd: %s\n", strerror(errno));
		return FILE_ERR_CLOSE;
	}
	return FILE_ERR_NONE;
}

enum file_err map_file(int fd, enum file_mode mode,
		       uint8_t **buf, uint32_t *len)
{
	struct stat sb;
	void *mmap_ptr;
	uint32_t reasonable_len;

	if (fstat(fd, &sb)!= 0) {
		ERROR("Can't stat input file: %s\n", strerror(errno));
		return FILE_ERR_STAT;
	}

#if !defined(HAVE_MACOS) && !defined(__FreeBSD__) && !defined(__OpenBSD__)
	if (S_ISBLK(sb.st_mode))
		ioctl(fd, BLKGETSIZE64, &sb.st_size);
#endif

	/* If the image is larger than 2^32 bytes, it's wrong. */
	if (sb.st_size < 0 || sb.st_size > UINT32_MAX) {
		ERROR("Image size is unreasonable\n");
		return FILE_ERR_SIZE;
	}
	reasonable_len = (uint32_t)sb.st_size;

	if (mode == FILE_RW)
		mmap_ptr = mmap(0, sb.st_size,
				PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	else
		mmap_ptr = mmap(0, sb.st_size,
				PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);

	if (mmap_ptr == MAP_FAILED) {
		ERROR("Can't mmap %s file: %s\n",
		      mode == FILE_RW ? "output" : "input", strerror(errno));
		return FILE_ERR_MMAP;
	}

	*buf = (uint8_t *)mmap_ptr;
	*len = reasonable_len;
	return FILE_ERR_NONE;
}

enum file_err unmap_file(int fd, enum file_mode mode,
			 uint8_t *buf, uint32_t len)
{
	void *mmap_ptr = buf;
	enum file_err err = FILE_ERR_NONE;

	if (mode == FILE_RW &&
	    (msync(mmap_ptr, len, MS_SYNC | MS_INVALIDATE) != 0)) {
		ERROR("msync failed: %s\n", strerror(errno));
		err = FILE_ERR_MSYNC;
	}

	if (munmap(mmap_ptr, len) != 0) {
		ERROR("Can't munmap pointer: %s\n", strerror(errno));
		if (err == FILE_ERR_NONE)
			err = FILE_ERR_MUNMAP;
	}

	return err;
}

enum file_err open_and_map_file(const char *infile, int *fd,
				enum file_mode mode, uint8_t **buf,
				uint32_t *len)
{
	enum file_err rv = open_file(infile, fd, mode);
	if (rv != FILE_ERR_NONE)
		return rv;

	rv = map_file(*fd, mode,  buf, len);
	if (rv != FILE_ERR_NONE)
		close_file(*fd);

	return rv;
}

enum file_err unmap_and_close_file(int fd, enum file_mode mode,
				   uint8_t *buf, uint32_t len)
{
	enum file_err rv = FILE_ERR_NONE;

	if (buf)
		rv = unmap_file(fd, mode, buf, len);
	if (rv != FILE_ERR_NONE)
		return rv;

	if (fd != -1)
		return close_file(fd);
	else
		return FILE_ERR_NONE;
}
