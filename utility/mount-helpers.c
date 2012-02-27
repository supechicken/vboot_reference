/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This is a collection of helper utilities for use with the "mount-encrypted"
 * utility.
 *
 */
#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <linux/fs.h>
#include <linux/loop.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "mount-encrypted.h"
#include "mount-helpers.h"

static const gchar * const kRootDir = "/";
static const gchar * const kLoopTemplate = "/dev/loop%d";
static const int kLoopMajor = 7;
static const int kLoopMax = 8;
static const unsigned int kResizeStepSeconds = 2;
static const size_t kResizeBlocks = 32768 * 10;
static const gchar * const kExt4ExtendedOptions = "discard,lazy_itable_init";

int remove_tree(const char *tree)
{
	const gchar *rm[] = {
		"/bin/rm", "-rf", tree,
		NULL
	};

	return runcmd(rm);
}

size_t get_sectors(const char *device)
{
	size_t sectors;
	int fd;
	if ((fd = open(device, O_RDONLY | O_NOFOLLOW)) < 0) {
		PERROR("open(%s)", device);
		return 0;
	}
	if (ioctl(fd, BLKGETSIZE, &sectors)) {
		PERROR("ioctl(%s, BLKGETSIZE)", device);
		return 0;
	}
	close(fd);
	return sectors;
}

int runcmd(const gchar *argv[])
{
	gint rc;
	gchar *out = NULL, *errout = NULL;
	GError *err = NULL;

	g_spawn_sync(kRootDir, (gchar **)argv, NULL, 0, NULL, NULL,
		     &out, &errout, &rc, &err);
	if (err) {
		ERROR("%s: %s", argv[0], err->message);
		g_error_free(err);
		return -1;
	}

	if (rc)
		ERROR("%s failed (%d)\n%s\n%s", argv[0], rc, out, errout);

	g_free(out);
	g_free(errout);

	return rc;
}

int same_vfs(const char *mnt_a, const char *mnt_b)
{
	struct statvfs stat_a, stat_b;

	if (statvfs(mnt_a, &stat_a)) {
		PERROR("statvfs(%s)", mnt_a);
		exit(1);
	}
	if (statvfs(mnt_b, &stat_b)) {
		PERROR("statvfs(%s)", mnt_b);
		exit(1);
	}
	return (stat_a.f_fsid == stat_b.f_fsid);
}

static int is_loop_device(int fd)
{
	struct stat info;

	return (fstat(fd, &info) == 0 && S_ISBLK(info.st_mode) &&
		major(info.st_rdev) == kLoopMajor);
}

static int loop_is_attached(int fd)
{
	struct loop_info info;

	errno = 0;
	if (ioctl(fd, LOOP_GET_STATUS, &info) && errno == ENXIO)
		return 0;

	return 1;
}

static int loop_allocate(gchar **loopback)
{
	int i, fd;

	*loopback = NULL;
	for (i = 0; i < kLoopMax; ++i) {
		g_free(*loopback);
		*loopback = g_strdup_printf(kLoopTemplate, i);
		if (!*loopback) {
			PERROR("g_strdup_printf");
			return -1;
		}

		fd = open(*loopback, O_RDONLY | O_NOFOLLOW);
		if (fd < 0) {
			PERROR("open(%s)", *loopback);
			goto failed;
		}
		if (is_loop_device(fd) && !loop_is_attached(fd)) {
			close(fd);
			fd = open(*loopback, O_RDWR | O_NOFOLLOW);
			if (is_loop_device(fd) && !loop_is_attached(fd))
				return fd;
		}
		close(fd);
	}
	ERROR("Ran out of loopback devices");

failed:
	g_free(*loopback);
	*loopback = NULL;
	return -1;
}

int loop_detach(const gchar *loopback)
{
	int fd;

	fd = open(loopback, O_RDONLY | O_NOFOLLOW);
	if (fd < 0) {
		PERROR("open(%s)", loopback);
		return 0;
	}
	if (!is_loop_device(fd) || !loop_is_attached(fd))
		goto failed;
	if (ioctl(fd, LOOP_CLR_FD, 0)) {
		PERROR("LOOP_CLR_FD");
		goto failed;
	}

	close (fd);
	return 1;

failed:
	close(fd);
	return 0;
}

gchar *loop_attach(int fd, const char *name)
{
	gchar *loopback = NULL;
	int loopfd;
	struct loop_info64 info;

	loopfd = loop_allocate(&loopback);
	if (loopfd < 0)
		return NULL;
	if (ioctl(loopfd, LOOP_SET_FD, fd) < 0) {
		PERROR("LOOP_SET_FD");
		goto failed;
	}

	memset(&info, 0, sizeof(info));
	strncpy((char*)info.lo_file_name, name, LO_NAME_SIZE);
	if (ioctl(loopfd, LOOP_SET_STATUS64, &info)) {
		PERROR("LOOP_SET_STATUS64");
		goto failed;
	}

	close(loopfd);
	close(fd);
	return loopback;
failed:
	close(loopfd);
	close(fd);
	g_free(loopback);
	return 0;
}

int dm_setup(size_t sectors, const gchar *encryption_key, const char *name,
		const gchar *dev, const char *path)
{
	/* Mount loopback device with dm-crypt using the encryption key. */
	gchar *table = g_strdup_printf("0 %zu crypt " \
				       "aes-cbc-essiv:sha256 %s " \
				       "0 %s 0 " \
				       "1 allow_discards",
				       sectors,
				       encryption_key,
				       dev);
	if (!table) {
		PERROR("g_strdup_printf");
		return 0;
	}

	const gchar *argv[] = {
		"/sbin/dmsetup",
		"create", name,
		"--noudevrules", "--noudevsync",
		"--table", table,
		NULL
	};

	/* TODO(keescook): replace with call to libdevmapper. */
	if (runcmd(argv) != 0) {
		g_free(table);
		return 0;
	}
	g_free(table);

	/* Make sure the dm-crypt device showed up. */
	if (access(path, R_OK)) {
		ERROR("%s does not exist", path);
		return 0;
	}

	return 1;
}

void dm_teardown(const char *path)
{
	const char *argv[] = {
		"/sbin/dmsetup",
		"remove", path,
		"--noudevrules", "--noudevsync",
		NULL
	};
	/* TODO(keescook): replace with call to libdevmapper. */
	runcmd(argv);
}

int sparse_create(const char *path, size_t size)
{
	int sparsefd;

	sparsefd = open(path, O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW,
			S_IRUSR | S_IWUSR);
	if (sparsefd < 0)
		goto out;

	if (ftruncate(sparsefd, size)) {
		int saved_errno = errno;

		close(sparsefd);
		unlink(path);
		errno = saved_errno;

		sparsefd = -1;
	}

out:
	return sparsefd;
}

int filesystem_build(const char *device, size_t block_bytes, size_t blocks_min,
			size_t blocks_max)
{
	int rc = 0;

	gchar *blocksize = g_strdup_printf("%zu", block_bytes);
	if (!blocksize) {
		PERROR("g_strdup_printf");
		goto out;
	}

	gchar *blocks_str;
	blocks_str = g_strdup_printf("%zu", blocks_min);
	if (!blocks_str) {
		PERROR("g_strdup_printf");
		goto free_blocksize;
	}

	gchar *extended;
	if (blocks_min < blocks_max) {
		extended = g_strdup_printf("%s,resize=%zu",
			kExt4ExtendedOptions, blocks_max);
	} else {
		extended = g_strdup_printf("%s", kExt4ExtendedOptions);
	}
	if (!extended) {
		PERROR("g_strdup_printf");
		goto free_blocks_str;
	}

	const gchar *mkfs[] = {
		"/sbin/mkfs.ext4",
		"-T", "default",
		"-b", blocksize,
		"-m", "0",
		"-O", "^huge_file,^flex_bg",
		"-E", extended,
		device,
		blocks_str,
		NULL
	};

	rc = (runcmd(mkfs) == 0);
	if (!rc)
		goto free_extended;

	const gchar *tune2fs[] = {
		"/sbin/tune2fs",
		"-c", "0",
		"-i", "0",
		device,
		NULL
	};
	rc = (runcmd(tune2fs) == 0);

free_extended:
	g_free(extended);
free_blocks_str:
	g_free(blocks_str);
free_blocksize:
	g_free(blocksize);
out:
	return rc;
}

/* Spawns a filesystem resizing process. */
void filesystem_resizer(const char *device, size_t blocks, size_t blocks_max)
{
	pid_t pid;

	/* Ignore resizing if we know the filesystem was built to max size. */
	if (blocks >= blocks_max)
		return;

	pid = fork();
	if (pid < 0) {
		PERROR("fork");
		return;
	}
	if (pid != 0) {
		INFO("Started filesystem resizing process.");
		return;
	}

	if (setsid() < 0) {
		PERROR("setsid");
		goto out;
	}

	INFO_INIT("Resizing started in %d second steps.", kResizeStepSeconds);

	do {
		gchar *blocks_str;

		sleep(kResizeStepSeconds);

		blocks += kResizeBlocks;
		if (blocks > blocks_max)
			blocks = blocks_max;

		blocks_str = g_strdup_printf("%zu", blocks);
		if (!blocks_str) {
			PERROR("g_strdup_printf");
			goto out;
		}

		const gchar *resize[] = {
			"/sbin/resize2fs",
			"-f",
			device,
			blocks_str,
			NULL
		};

		INFO("Resizing filesystem on %s to %zu.", device, blocks);
		if (runcmd(resize)) {
			ERROR("resize2fs failed");
			goto out;
		}
		g_free(blocks_str);
	} while (blocks < blocks_max);

	INFO("Resizing finished.");
out:
	exit(0);
}
