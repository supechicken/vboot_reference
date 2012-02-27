/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This tool will attempt to mount or create the encrypted stateful partition,
 * and the various bind mountable subdirectories.
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
#include <grp.h>
#include <pwd.h>
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

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#define DEBUG_ENABLED 0

#define CHROMEOS_ENVIRONMENT
#include "tlcl.h"
#include "crossystem.h"

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

#define STATEFUL_PARTITION "/mnt/stateful_partition"
#define ENCRYPTED_PARTITION STATEFUL_PARTITION "/encrypted"
#define DMCRYPT_DEV_NAME "encstateful"
#define BUF_SIZE 1024
#define DIGEST_LENGTH SHA256_DIGEST_LENGTH

const gchar * const kRootDir = "/";
const gchar * const kKernelCmdline = "/proc/cmdline";
const gchar * const kKernelCmdlineOption = "encrypted-stateful-key=";
const gchar * const kStatefulPartition = STATEFUL_PARTITION;
const gchar * const kEncryptedKey = STATEFUL_PARTITION "/encrypted.key";
const gchar * const kEncryptedBlock = STATEFUL_PARTITION "/encrypted.block";
const gchar * const kEncryptedPartition = ENCRYPTED_PARTITION;
const gchar * const kEncryptedFS = "ext4";
const gchar * const kCryptName = DMCRYPT_DEV_NAME;
const gchar * const kCryptPath = "/dev/mapper/" DMCRYPT_DEV_NAME;
const gchar * const kTpmPath = "/dev/tpm0";
const gchar * const kNullPath = "/dev/null";
const float kSizePercent = 0.3;
const uint32_t kLockboxIndex = 0x20000004;
const uint32_t kLockboxSizeV1 = 0x2c;
const uint32_t kLockboxSizeV2 = 0x45;
const uint32_t kLockboxSaltOffset = 0x5;
const size_t kSectorSize = 512;
const size_t kExt4BlockSize = 4096;
const size_t kExt4MinBytes = 64 * 1024 * 1024;
const unsigned int kResizeStepSeconds = 2;
const size_t kResizeBlocks = 32768 * 10;
const gchar * const kLoopTemplate = "/dev/loop%d";
const int kLoopMajor = 7;
const int kLoopMax = 8;

static struct bind_mount {
	char *src;
	char *old;
	char *dst;
	char *owner;
	char *group;
	mode_t mode;
} bind_mounts[] = {
#if DEBUG_ENABLED
# define DEBUG_DEST ".new"
#else
# define DEBUG_DEST ""
#endif
	{ ENCRYPTED_PARTITION "/var", STATEFUL_PARTITION "/var",
	  "/var" DEBUG_DEST, "root", "root",
	  S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH },
	{ ENCRYPTED_PARTITION "/chronos", "/home/chronos",
	  "/home/chronos" DEBUG_DEST, "chronos", "chronos",
	  S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH },
	{ },
};

int has_tpm = 0;

#if DEBUG_ENABLED
static void debug_dump_hex(const char *name, uint8_t *data, uint32_t size)
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

static void sha256(char *string, uint8_t *digest)
{
	SHA256((unsigned char *)string, strlen(string), digest);
}

static int get_key_from_cmdline(uint8_t *digest)
{
	int result = 0;
	gchar *cmdline;
	gsize length;
	char *item;

	if (!g_file_get_contents(kKernelCmdline, &cmdline, &length, NULL)) {
		PERROR(kKernelCmdline);
		return 0;
	}

	for (item = strtok(cmdline, " "); item; item = strtok(NULL, " ")) {
		if (strcmp(item, kKernelCmdlineOption) == 0) {
			sha256(item + strlen(kKernelCmdlineOption), digest);
			debug_dump_hex("system key", digest, DIGEST_LENGTH);
			result = 1;
			break;
		}
	}

	g_free(cmdline);
	return result;
}

static int is_chromeos(void)
{
	int result = 0;
	char fw[64];

	fw[0] = '\0';
	VbGetSystemPropertyString("mainfw_type", fw, sizeof(fw));
	if (strcmp(fw, "nonchrome") != 0)
		result = 1;

	return result;
}

static int is_cr48(void)
{
	int result = 0;
	char hwid[64];

	hwid[0] = '\0';
	VbGetSystemPropertyString("hwid", hwid, sizeof(hwid));
	if (strstr(hwid, "MARIO"))
		result = 1;

	return result;
}

static int
_read_nvram(uint8_t *buffer, size_t len, uint32_t index, uint32_t size)
{
	if (size > len) {
		ERROR("NVRAM size (0x%x > 0x%zx) is too big", size, len);
		return 0;
	}

	return TlclRead(index, buffer, size);
}

/*
 * Cases:
 *  - no NVRAM area at all (OOBE)
 *  - defined NVRAM area, but TPM not Owned
 *  - defined NVRAM area, but not Finalized
 *  - legacy NVRAM area (migration needed)
 *  - modern NVRAM area (\o/)
 */
// TODO(keescook): recovery code needs to wipe NVRAM area to new size?
static int get_nvram_key(uint8_t *digest, int *old_lockbox)
{
	TPM_PERMANENT_FLAGS pflags;
	uint8_t value[4096], all_ff = 1, all_zero = 1;
	uint32_t size, result, i;
	uint8_t *rand_bytes;
	uint32_t rand_size;

	/* Start by expecting modern NVRAM area. */
	*old_lockbox = 0;
	size = kLockboxSizeV2;
	result = _read_nvram(value, sizeof(value), kLockboxIndex, size);
	if (result) {
		size = kLockboxSizeV1;
		result = _read_nvram(value, sizeof(value), kLockboxIndex, size);
		if (result) {
			/* No NVRAM area at all. */
			INFO("No NVRAM area defined.");
			return 0;
		}
		/* Legacy NVRAM area. */
		INFO("Legacy NVRAM area found.");
		*old_lockbox = 1;
	} else {
		INFO("NVRAM area found.");
	}

	debug_dump_hex("nvram", value, size);

	/* Ignore defined but unowned NVRAM area. */
	result = TlclGetPermanentFlags(&pflags);
	if (result) {
		INFO("Could not read TPM Permanent Flags.");
		return 0;
	}
	if (!pflags.ownership) {
		INFO("TPM not Owned, ignoring NVRAM area.");
		return 0;
	}

	/* Ignore defined but unwritten NVRAM area. */
	for (i = 0; i < size; ++i) {
		if (all_zero && value[i] != 0x00)
			all_zero = 0;
		if (all_ff && value[i] != 0xff)
			all_ff = 0;
	}
	if (all_zero || all_ff) {
		INFO("NVRAM area has been defined but not written.");
		return 0;
	}

	/* Choose random bytes to use based on NVRAM version. */
	if (*old_lockbox) {
		rand_bytes = value;
		rand_size = size;
	} else {
		rand_bytes = value + kLockboxSaltOffset;
		if (kLockboxSaltOffset + DIGEST_LENGTH > size) {
			INFO("Impossibly small NVRAM area size (%d).", size);
			return 0;
		}
		rand_size = DIGEST_LENGTH;
	}
	if (rand_size < DIGEST_LENGTH) {
		INFO("Impossibly small rand_size (%d).", rand_size);
		return 0;
	}
	debug_dump_hex("rand_bytes", rand_bytes, rand_size);

	SHA256(rand_bytes, rand_size, digest);
	debug_dump_hex("system key", digest, DIGEST_LENGTH);

	return 1;
}

static int find_system_key(uint8_t *digest, int *migration_allowed)
{
	gchar *key;
	gsize length;

	/* By default, do not allow migration. */
	*migration_allowed = 0;
	/* CR48 is excluded because it lacks the NVRAM area. */
	if (is_chromeos() && !is_cr48()) {
		INFO("Using NVRAM as system key.");
		return get_nvram_key(digest, migration_allowed);
	}

	if (get_key_from_cmdline(digest)) {
		INFO("Using kernel command line argument as system key.");
		return 1;
	}
	if (g_file_get_contents("/sys/class/dmi/id/product_uuid",
				&key, &length, NULL)) {
		sha256(key, digest);
		debug_dump_hex("system key", digest, DIGEST_LENGTH);
		g_free(key);
		INFO("Using UUID as system key.");
		return 1;
	}

	INFO("Using default insecure system key.");
	sha256("default unsafe static key", digest);
	debug_dump_hex("system key", digest, DIGEST_LENGTH);
	return 1;
}

/* Returns allocated string that holds [length]*2 + 1 characters. */
static char *stringify_hex(uint8_t *binary, size_t length)
{
	char *string;
	size_t i;

	string = malloc(DIGEST_LENGTH * 2 + 1);
	if (!string) {
		PERROR("malloc");
		return NULL;
	}
	for (i = 0; i < length; ++i)
		sprintf(string + (i * 2), "%02x", binary[i]);
	string[length * 2] = '\0';

	return string;
}

static char *get_encryption_key(uint8_t *system_key)
{
	char *key = NULL;
	unsigned char *cipher = NULL;
	gsize length;
	uint8_t *plain = NULL;
	int plain_length, final_len;
	EVP_CIPHER_CTX ctx;

	if (g_access(kEncryptedKey, R_OK)) {
		/* This file being missing is handled in caller, so
		 * do not emit error message.
		 */
		INFO("%s does not exist.", kEncryptedKey);
		goto out;
	}

	if (!g_file_get_contents(kEncryptedKey, (gchar **)&cipher, &length,
				 NULL)) {
		PERROR(kEncryptedKey);
		goto out;
	}
	plain = malloc(length);
	if (!plain) {
		PERROR("malloc");
		goto free_cipher;
	}

	/* Use the default IV. */
	/* TODO(keescook): how do I verify that DIGEST_SIZE == key size? */
	if (!EVP_DecryptInit(&ctx, EVP_aes_256_cbc(), system_key, NULL)) {
		SSL_ERROR("EVP_DecryptInit");
		goto free_plain;
	}
	if (!EVP_DecryptUpdate(&ctx, plain, &plain_length, cipher, length)) {
		SSL_ERROR("EVP_DecryptUpdate");
		goto free_ctx;
	}
	if (!EVP_DecryptFinal(&ctx, plain+plain_length, &final_len)) {
		SSL_ERROR("EVP_DecryptFinal");
		goto free_ctx;
	}
	plain_length += final_len;

	if (plain_length != DIGEST_LENGTH) {
		ERROR("Decrypted encryption key length (%d) is not %d",
		      plain_length, DIGEST_LENGTH);
		goto free_ctx;
	}

	debug_dump_hex("encryption key", plain, DIGEST_LENGTH);

	key = stringify_hex(plain, DIGEST_LENGTH);

free_ctx:
	EVP_CIPHER_CTX_cleanup(&ctx);
free_plain:
	free(plain);
free_cipher:
	g_free(cipher);
out:
	return key;
}

/* Returns 1 on success, 0 on failure. */
static int get_random_bytes_tpm(unsigned char *buffer, int wanted)
{
	uint32_t remaining = wanted;

	/* Read random bytes from TPM, which can return short reads. */
	while (remaining) {
		uint32_t result, size;

		result = TlclGetRandom(buffer + (wanted - remaining),
				       remaining, &size);
		if (result || size > remaining) {
			ERROR("TPM GetRandom failed.");
			return 0;
		}
		remaining -= size;
	}

	return 1;
}

/* Returns 1 on success, 0 on failure. */
static int get_random_bytes(unsigned char *buffer, int wanted)
{
	if (has_tpm)
		return get_random_bytes_tpm(buffer, wanted);
	else
		return RAND_bytes(buffer, wanted);
}

static char *choose_encryption_key(void)
{
	unsigned char rand_bytes[DIGEST_LENGTH];
	unsigned char digest[DIGEST_LENGTH];

	get_random_bytes(rand_bytes, sizeof(rand_bytes));

	SHA256(rand_bytes, DIGEST_LENGTH, digest);
	debug_dump_hex("encryption key", digest, DIGEST_LENGTH);

	return stringify_hex(digest, DIGEST_LENGTH);
}

static int runcmd(const gchar *argv[])
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

static int check_bind_src(struct bind_mount *bind)
{
	struct passwd *user;
	struct group *group;

	if (access(bind->src, R_OK) == 0)
		return 0;

	if (!(user = getpwnam(bind->owner))) {
		PERROR("getpwnam(%s)", bind->owner);
		return -1;
	}
	if (!(group = getgrnam(bind->group))) {
		PERROR("getgrnam(%s)", bind->group);
		return -1;
	}

	if (mkdir(bind->src, bind->mode)) {
		PERROR("mkdir(%s)", bind->src);
		return -1;
	}
	/* Must do explicit chmod since mkdir()'s mode respects umask. */
	if (chmod(bind->src, bind->mode)) {
		PERROR("chmod(%s)", bind->src);
		return -1;
	}
	if (chown(bind->src, user->pw_uid, group->gr_gid)) {
		PERROR("chown(%s)", bind->src);
		return -1;
	}

	return 0;
}

/* Spawns a filesystem resizing process. */
static void resize_filesystem(size_t blocks_min, size_t blocks_max)
{
	size_t blocks = blocks_min;
	pid_t pid;

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

		const gchar *mkfs[] = {
			"/sbin/resize2fs",
			"-f",
			kCryptPath,
			blocks_str,
			NULL
		};

		INFO("Resizing filesystem on %s to %zu.", kCryptPath, blocks);
		if (runcmd(mkfs)) {
			ERROR("resize2fs failed");
			goto out;
		}
		g_free(blocks_str);
	} while (blocks < blocks_max);

	INFO("Resizing finished.");
out:
	exit(0);
}

static int build_filesystem(size_t sectors, int noresize)
{
	int rc = 0;
	size_t blocks_min = kExt4MinBytes / kExt4BlockSize;
	size_t blocks_max = sectors / (kExt4BlockSize / kSectorSize);

	gchar *blocksize = g_strdup_printf("%zu", kExt4BlockSize);
	if (!blocksize) {
		PERROR("g_strdup_printf");
		goto out;
	}

	gchar *blocks_str;
	blocks_str = g_strdup_printf("%zu",
				     noresize ? blocks_max : blocks_min);
	if (!blocks_str) {
		PERROR("g_strdup_printf");
		goto free_blocksize;
	}

	gchar *extended;
	extended = g_strdup_printf("discard,lazy_itable_init,resize=%zu",
				   blocks_max);
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
		kCryptPath,
		blocks_str,
		NULL
	};

	INFO("Building filesystem on %s.", kCryptPath);
	rc = (runcmd(mkfs) == 0);
	if (!rc)
		goto free_extended;

	const gchar *tune2fs[] = {
		"/sbin/tune2fs",
		"-c", "0",
		"-i", "0",
		kCryptPath,
		NULL
	};
	INFO("Tuning filesystem on %s.", kCryptPath);
	rc = (runcmd(tune2fs) == 0);

	if (rc && !noresize)
		resize_filesystem(blocks_min, blocks_max);

free_extended:
	g_free(extended);
free_blocks_str:
	g_free(blocks_str);
free_blocksize:
	g_free(blocksize);
out:
	return rc;
}

static size_t get_sectors(char *device)
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

static int remove_tree(char *tree)
{
	const gchar *rm[] = {
		"/bin/rm", "-rf", tree,
		NULL
	};

	return runcmd(rm);
}

static void migrate_contents(struct bind_mount *bind)
{
	gchar *old;

	/* Skip migration if the old bind src is missing. */
	if (!bind->old || access(bind->old, R_OK))
		return;

	INFO("Migrating bind mount src %s to %s.", bind->old, bind->src);
	check_bind_src(bind);

	if (!(old = g_strdup_printf("%s/.", bind->old))) {
		PERROR("g_strdup_printf");
		goto remove;
	}

	const gchar *cp[] = {
		"/bin/cp", "-a",
		old,
		bind->src,
		NULL
	};

	if (runcmd(cp) != 0) {
		/* If the copy failed, it may have partially populated the
		 * new source, so we need to remove the new source and
		 * rebuild it. Regardless, the old source must be removed
		 * as well.
		 */
		INFO("Failed to migrate %s to %s!", bind->old, bind->src);
		remove_tree(bind->src);
		check_bind_src(bind);
	}

remove:
	g_free(old);

#if DEBUG_ENABLED
	INFO("Want to remove %s.", bind->old);
	return;
#endif
	remove_tree(bind->old);
	return;
}

static int is_loop_device(int fd)
{
	struct stat info;

	return (fstat(fd, &info) == 0 && S_ISBLK(info.st_mode) &&
		major(info.st_rdev) == kLoopMajor);
}

static int is_loop_attached(int fd)
{
	struct loop_info info;

	errno = 0;
	if (ioctl(fd, LOOP_GET_STATUS, &info) && errno == ENXIO)
		return 0;

	return 1;
}

int allocate_loopback(gchar **loopback)
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
		if (is_loop_device(fd) && !is_loop_attached(fd)) {
			close(fd);
			fd = open(*loopback, O_RDWR | O_NOFOLLOW);
			if (is_loop_device(fd) && !is_loop_attached(fd))
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

static int unloop(gchar *loopback)
{
	int fd;

	fd = open(loopback, O_RDONLY | O_NOFOLLOW);
	if (fd < 0) {
		PERROR("open(%s)", loopback);
		return 0;
	}
	if (!is_loop_device(fd) || !is_loop_attached(fd))
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

static gchar *attach_loopback(int sparsefd, const char *sparsepath)
{
	gchar *loopback = NULL;
	int loopfd;
	struct loop_info64 info;

	loopfd = allocate_loopback(&loopback);
	if (loopfd < 0)
		return NULL;
	if (ioctl(loopfd, LOOP_SET_FD, sparsefd) < 0) {
		PERROR("LOOP_SET_FD");
		goto failed;
	}

	memset(&info, 0, sizeof(info));
	strncpy((char*)info.lo_file_name, sparsepath, LO_NAME_SIZE);
	if (ioctl(loopfd, LOOP_SET_STATUS64, &info)) {
		PERROR("LOOP_SET_STATUS64");
		goto failed;
	}

	close(loopfd);
	close(sparsefd);
	return loopback;
failed:
	close(loopfd);
	close(sparsefd);
	g_free(loopback);
	return 0;
}

static int setup_encrypted(void)
{
	uint8_t system_key[DIGEST_LENGTH];
	char *encryption_key = NULL;
	int migrate = 0, rebuild = 0;
	gchar *lodev = NULL;
	size_t sectors;
	struct bind_mount *bind;
	int sparsefd;

	/* Use the "system key" to decrypt the "encryption key" stored in
	 * the stateful partition.
	 */
	if (find_system_key(system_key, &migrate)) {
		encryption_key = get_encryption_key(system_key);
	} else {
		INFO("No usable system key found.");
	}

	if (encryption_key) {
		migrate = 0;
	} else {
		INFO("Generating new encryption key.");
		encryption_key = choose_encryption_key();
		if (!encryption_key)
			return 0;
		rebuild = 1;
	}

	if (rebuild) {
		struct statvfs buf;
		off_t size;

		/* Wipe out the old files, and ignore errors. */
		unlink(kEncryptedKey);
		unlink(kEncryptedBlock);

		/* Calculate the desired size of the new partition. */
		if (statvfs(kStatefulPartition, &buf)) {
			PERROR(kStatefulPartition);
			return 0;
		}
		size = buf.f_blocks;
		size *= kSizePercent;
		size *= buf.f_frsize;

		INFO("Creating sparse backing file with size %llu.",
		     (unsigned long long)size);

		/* Create the sparse file. */
		sparsefd = open(kEncryptedBlock,
				O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW,
				S_IRUSR | S_IWUSR);
		if (sparsefd < 0) {
			PERROR(kEncryptedBlock);
			return 0;
		}
		if (ftruncate(sparsefd, size)) {
			PERROR("truncate");
			return 0;
		}

		// TODO(keescook): non-cros machines need to write the
		// wrapped file here.
	} else {
		sparsefd = open(kEncryptedBlock, O_RDWR | O_NOFOLLOW);
		if (sparsefd < 0) {
			PERROR(kEncryptedBlock);
			return 0;
		}
	}

	/* Set up loopback device. */
	lodev = attach_loopback(sparsefd, kEncryptedBlock);
	if (!lodev || strlen(lodev) == 0) {
		ERROR("attach_loopback failed");
		goto failed;
	}
	INFO("Loopback attached %s as %s.", kEncryptedBlock, lodev);

	/* Get size as seen by block device. */
	sectors = get_sectors(lodev);
	if (!sectors) {
		ERROR("Failed to read device size");
		goto losetdown;
	}

	/* Mount loopback device with dm-crypt using the encryption key. */
	{
		gchar *table = g_strdup_printf("0 %zu crypt " \
					       "aes-cbc-essiv:sha256 %s " \
					       "0 %s 0 " \
					       "1 allow_discards",
					       sectors,
					       encryption_key,
					       lodev);
		if (!table) {
			PERROR("g_strdup_printf");
			goto losetdown;
		}

		const gchar *argv[] = {
			"/sbin/dmsetup",
			"create", kCryptName,
			"--noudevrules", "--noudevsync",
			"--table", table,
			NULL
		};

		/* TODO(keescook): replace with call to libdevmapper. */
		INFO("Setting up dm-crypt %s as %s.", lodev, kCryptPath);
		if (runcmd(argv) != 0) {
			g_free(table);
			goto losetdown;
		}
		g_free(table);
	}
	/* Make sure the dm-crypt device showed up. */
	if (access(kCryptPath, R_OK)) {
		ERROR("%s does not exist", kCryptPath);
		goto losetdown;
	}

	/* Decide now if any migration will happen. If so, we will not
	 * grow the new filesystem in the background, since we need to
	 * copy the contents over before /var is valid again.
	 */
	if (!rebuild)
		migrate = 0;
	if (migrate) {
		int needed = 0;

		for (bind = bind_mounts; bind->src; ++ bind) {
			/* Skip mounts that have no prior location defined. */
			if (!bind->old)
				continue;
			/* Skip mounts that have no prior data on disk. */
			if (access(bind->old, R_OK) != 0)
				continue;

			needed = 1;
		}
		migrate = needed;
	}

	/* Build the ext4 filesystem. */
	if (rebuild && !build_filesystem(sectors, migrate))
		goto dmsetdown;

	/* Mount the dm-crypt partition finally. */
	INFO("Mounting %s onto %s.", kCryptPath, kEncryptedPartition);
	if (access(kEncryptedPartition, R_OK) &&
	    mkdir(kEncryptedPartition, S_IRWXU | S_IRWXG | \
				       S_IROTH | S_IXOTH)) {
		PERROR(kCryptPath);
		goto dmsetdown;
	}
	if (mount(kCryptPath, kEncryptedPartition, kEncryptedFS,
		  MS_NODEV | MS_NOEXEC | MS_NOSUID | MS_RELATIME,
		  "discard")) {
		PERROR("mount(%s,%s)", kCryptPath, kEncryptedPartition);
		goto dmsetdown;
	}

	/* If the legacy lockbox NVRAM area exists, we've rebuilt the
	 * filesystem, and there are old bind sources on disk, attempt
	 * migration.
	 */
	if (migrate) {
		/* Migration needs to happen before bind mounting because
		 * some partitions were not already on the stateful partition,
		 * and would be over-mounted by the new bind mount.
		 */
		for (bind = bind_mounts; bind->src; ++ bind)
			migrate_contents(bind);
	}

	/* Perform bind mounts. */
	for (bind = bind_mounts; bind->src; ++ bind) {
		INFO("Bind mounting %s onto %s.", bind->src, bind->dst);
		if (check_bind_src(bind) ||
		    mount(bind->src, bind->dst, "none", MS_BIND, NULL)) {
			PERROR("mount(%s,%s)", bind->src, bind->dst);
			goto unbind;
		}
	}

	free(lodev);
	return 1;

unbind:
	for (bind = bind_mounts; bind->src; ++ bind) {
		INFO("Unmounting %s.", bind->dst);
		umount(bind->dst);
	}

	INFO("Unmounting %s.", kEncryptedPartition);
	umount(kEncryptedPartition);

dmsetdown:
	{
		/* TODO(keescook): syscall/helper. */
		const char *argv[] = {
			"/sbin/dmsetup",
			"remove", kCryptPath,
			"--noudevrules", "--noudevsync",
			NULL
		};
		INFO("Removing %s.", kCryptPath);
		runcmd(argv);
	}

losetdown:
	INFO("Unlooping %s.", lodev);
	unloop(lodev);

failed:
	free(lodev);

	return 0;
}

static int same_vfs(const char *mnt_a, const char *mnt_b)
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

static void sanity_check(void)
{
	struct bind_mount *bind;

	/* Verify stateful partition exists and is mounted. */
	if (access(kStatefulPartition, R_OK) ||
	    same_vfs(kStatefulPartition, kRootDir)) {
		INFO("%s is not mounted.", kStatefulPartition);
		exit(1);
	}

	/* Verify encrypted partition is missing or not already mounted. */
	if (access(kEncryptedPartition, R_OK) == 0 &&
	    !same_vfs(kEncryptedPartition, kStatefulPartition)) {
		INFO("%s already appears to be mounted.", kEncryptedPartition);
		exit(0);
	}

	/* Verify that old bind mounts on stateful haven't happened yet. */
	for (bind = bind_mounts; bind->src; ++ bind) {
		if (strstr(bind->old, kStatefulPartition) != bind->old)
			continue;

		if (same_vfs(bind->dst, kStatefulPartition)) {
			INFO("%s already bind mounted.", bind->dst)
			exit(1);
		}
	}
	INFO("VFS sanity check ok.");
}

int status(void)
{
	uint8_t system_key[DIGEST_LENGTH];
	TPM_PERMANENT_FLAGS pflags;
	int old_lockbox = -1;

	printf("TPM: %s\n", has_tpm ? "yes" : "no");
	if (has_tpm) {
		printf("TPM Owned: %s\n", TlclGetPermanentFlags(&pflags) ?
			"fail" : (pflags.ownership ? "yes" : "no"));
	}
	printf("ChromeOS: %s\n", is_chromeos() ? "yes" : "no");
	printf("CR48: %s\n", is_cr48() ? "yes" : "no");
	if (is_chromeos() && !is_cr48()) {
		int rc;
		rc = get_nvram_key(system_key, &old_lockbox);
		if (old_lockbox == -1)
			printf("NVRAM: missing\n");
		else {
			printf("NVRAM: %s, %s\n",
				old_lockbox ? "legacy" : "modern",
				rc ? "available" : "ignored");
		}
	}
	else {
		printf("NVRAM: not present\n");
	}

	return 0;
}

void init_tpm(void)
{
	int tpm;

	tpm = open(kTpmPath, O_RDWR);
	if (tpm >= 0) {
		has_tpm = 1;
		close(tpm);
	}
	else {
		/* TlclLibInit does not fail, it exits, so instead,
		 * have it open /dev/null if the TPM is not available.
		 */
		setenv("TPM_DEVICE_PATH", kNullPath, 1);
	}
	TlclLibInit();
}

int main(int argc, char *argv[])
{
	int okay;

	INFO_INIT("Starting.");
	init_tpm();

	if (argc > 1 && !strcmp(argv[1], "status"))
		return status();

	sanity_check();

	okay = setup_encrypted();
	if (!okay) {
		INFO("Setup failed -- clearing files and retrying.");
		unlink(kEncryptedKey);
		unlink(kEncryptedBlock);
		okay = setup_encrypted();
	}

	INFO("Done.");

	// Continue boot.
	return okay ? 0 : 1;
}
