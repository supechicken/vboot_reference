/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Header file for mount helpers.
 */
#ifndef _MOUNT_HELPERS_H_
#define _MOUNT_HELPERS_H_

/* General utility functions. */
size_t get_sectors(const char *device);
int remove_tree(const char *tree);
int runcmd(const gchar *argv[]);
int same_vfs(const char *mnt_a, const char *mnt_b);

/* Loopback device attach/detach helpers. */
gchar *loop_attach(int fd, const char *name);
int loop_detach(const gchar *loopback);

/* Encrypted device mapper setup/teardown. */
int dm_setup(size_t sectors, const gchar *encryption_key, const char *name,
		const gchar *dev, const char *path);
void dm_teardown(const char *path);

/* Sparse file creation. */
int sparse_create(const char *path, size_t size);

/* Filesystem creation. */
int filesystem_build(const char *device, size_t block_bytes, size_t blocks_min,
			size_t blocks_max);
void filesystem_resizer(const char *device, size_t blocks, size_t blocks_max);

#endif /* _MOUNT_HELPERS_H_ */
