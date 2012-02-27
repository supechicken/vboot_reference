/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Private header file for mount-encrypted helper tool.
 */
#ifndef _MOUNT_ENCRYPTED_H_
#define _MOUNT_ENCRYPTED_H_

// TODO(keescook): Disable debugging in production.
#define DEBUG_ENABLED 0

#define _ERROR(f, a...)	{ \
	fprintf(stderr, "ERROR %s (%s, %d): ", \
			__func__, __FILE__, __LINE__); \
	fprintf(stderr, f, ## a); \
}
#define ERROR(f, a...)	{ \
	_ERROR(f, ## a); \
	fprintf(stderr, "\n"); \
}
#define PERROR(f, a...)	{ \
	_ERROR(f, ## a); \
	fprintf(stderr, ": %s\n", strerror(errno)); \
}

#define SSL_ERROR(f, a...)	{ \
	ERR_load_crypto_strings(); \
	_ERROR(f, ## a); \
	fprintf(stderr, "%s\n", ERR_error_string(ERR_get_error(), NULL)); \
}

#if DEBUG_ENABLED
static struct timeval tick;
# define TICK_INIT() gettimeofday(&tick, NULL)
# ifdef DEBUG_TIME_DELTA
#  define TICK_REPORT() { \
	struct timeval now, diff; \
	gettimeofday(&now, NULL); \
	diff.tv_sec = now.tv_sec - tick.tv_sec; \
	if (tick.tv_usec > now.tv_usec) { \
		diff.tv_sec -= 1; \
		diff.tv_usec = 1000000 - tick.tv_usec + now.tv_usec; \
	} else { \
		diff.tv_usec = now.tv_usec - tick.tv_usec; \
	} \
	tick = now; \
	printf("\tTook: [%2d.%06d]\n", (int)diff.tv_sec, (int)diff.tv_usec); \
}
# else
#  define TICK_REPORT() { \
	gettimeofday(&tick, NULL); \
	printf("[%2d.%06d] ", (int)tick.tv_sec, (int)tick.tv_usec); \
}
# endif
#else
# define TICK_INIT() { }
# define TICK_REPORT() { }
#endif

#define INFO_INIT(f, a...) { \
	TICK_INIT(); \
	printf(f, ## a); \
	printf("\n"); \
}
#define INFO(f, a...) { \
	TICK_REPORT(); \
	printf(f, ## a); \
	printf("\n"); \
}
#if DEBUG_ENABLED
# define DEBUG(f, a...) { \
	printf(f, ## a); \
	printf("\n"); \
}
#else
# define DEBUG(f, a...) do { } while (0)
#endif

#if DEBUG_ENABLED
static inline void debug_dump_hex(const char *name, uint8_t *data,
				  uint32_t size)
{
	int i;
	printf("%s: ", name);
	for (i = 0; i < size; i++) {
		printf("%02x ", data[i]);
	}
	printf("\n");
}
#else
# define debug_dump_hex(n, d, s) do { } while (0)
#endif

#endif /* _MOUNT_ENCRYPTED_H_ */
