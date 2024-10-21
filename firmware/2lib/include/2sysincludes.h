/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * System includes for vboot reference library.  This is the ONLY
 * place in firmware/ where system headers may be included via
 * #include <...>, so that there's only one place that needs to be
 * fixed up for platforms which don't have all the system includes.
 */

#ifndef VBOOT_REFERENCE_2SYSINCLUDES_H_
#define VBOOT_REFERENCE_2SYSINCLUDES_H_

#include <ctype.h>
#include <inttypes.h>  /* For PRIu64 */
<<<<<<< HEAD   (425ede 2lib: Add gbb flag to enforce CSE sync)
||||||| BASE
#include <stdbool.h>
=======
#include <stdarg.h>
#include <stdbool.h>
>>>>>>> CHANGE (911e5a avb: Implement basic AVB callbacks)
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#endif  /* VBOOT_REFERENCE_2SYSINCLUDES_H_ */
