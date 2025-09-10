/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Host-side temporary file creation functions.
 */

#ifndef VBOOT_REFERENCE_TEMPFILE_H_
#define VBOOT_REFERENCE_TEMPFILE_H_

#include <stddef.h>  /* for size_t */

/**
 * Create a temporary file in VBOOT_TMP_DIR.
 *
 * The file is created securely using mkstemp(). The final filename is copied
 * back into the template buffer.
 *
 * @param path_template   A buffer containing a filename template, ending in
 *                        "XXXXXX". This buffer will be modified in-place to
 *                        contain the actual filename.
 * @param template_size   The total size of the path_template buffer.
 *
 * @return the file descriptor on success, or -1 on error.
 */
int create_vboot_temp_file(char *path_template, size_t template_size);

#endif  /* VBOOT_REFERENCE_TEMPFILE_H_ */
