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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#define CHROMEOS_ENVIRONMENT
#include "tlcl.h"

/* FIXME(keescook): turn this off in production. */
#define DEBUG_ENABLED 1

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

#define INFO(f, a...) { \
	printf(f, ## a); \
	printf("\n"); \
}
#ifdef DEBUG_ENABLED
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
const gchar * const kCryptName = DMCRYPT_DEV_NAME;
const gchar * const kCryptPath = "/dev/mapper/" DMCRYPT_DEV_NAME;
const float kSizePercent = 0.3;
const uint32_t kLockboxIndex = 0x20000004;
const uint32_t kLockboxSizeV1 = 0x2c;
const uint32_t kLockboxSizeV2 = 0x4c;

struct bind_mount {
	char *src;
	char *dst;
	int from_stateful; /* Should migration happen from stateful? */
	char *owner;
	char *group;
	mode_t mode;
} bind_mounts[] = {
	{ ENCRYPTED_PARTITION "/var", "/var.new", 1,
	  "root", "root",
	  S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH },
	{ ENCRYPTED_PARTITION "/chronos", "/home/chronos", 0,
	  "chronos", "chronos",
	  S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH },
	{ },
};

#ifdef DEBUG_ENABLED
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
# define debug_dump_hex(d, s) do { } while (0)
#endif

static void sha256(char *string, uint8_t *digest)
{
	SHA256((unsigned char *)string, strlen(string), digest);
}

char *read_process_line(char *cmdline)
{
	FILE *pipe;
	char *buffer;
	int size;

	/* FIXME(keescook): replace this with g_spawn_sync(). */
	if (!(pipe = popen(cmdline, "r"))) {
		PERROR("'%s'", cmdline);
		return NULL;
	}
	buffer = malloc(BUF_SIZE);
	if (!buffer) {
		PERROR("malloc");
		goto failed;
	}

	if (!fgets(buffer, BUF_SIZE, pipe)) {
		PERROR("fgets");
		goto failed;
	}
	size = strlen(buffer);
	if (size && buffer[size - 1] == '\n')
		buffer[size - 1] = '\0';

	fclose(pipe);
	return buffer;

failed:
	free(buffer);
	if (pipe)
		fclose(pipe);
	return NULL;
}

int get_key_from_cmdline(uint8_t *digest)
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
			result = 1;
			break;
		}
	}

	g_free(cmdline);
	return result;
}

int is_chromeos(void)
{
	char *fw;

	fw = read_process_line("crossystem mainfw_type");
	if (!fw)
		return 0;
	if (strcmp(fw, "nonchrome") == 0)
		return 0;

	free(fw);
	return 1;
}

int is_cr48(void)
{
	int result = 0;
	char *hwid;

	hwid = read_process_line("crossystem hwid");
	if (hwid && strstr(hwid, "MARIO"))
		result = 1;
	free(hwid);

	return result;
}

static int
_read_nvram(uint8_t *buffer, size_t len, uint32_t index, uint32_t size)
{
	if (size > len) {
		ERROR("NVRAM size (0x%x > 0x%x) is too big", size, len);
		return 0;
	}

	/* This is safe to call more than once. */
	TlclLibInit();

	return TlclRead(index, buffer, size);
}

/*
 * Cases:
 *  - no NVRAM area at all (OOBE)
 *  - legacy NVRAM area (migration needed)
 *  - modern NVRAM area (\o/)
 */
int get_nvram_key(uint8_t *digest, int *old_lockbox)
{
	uint8_t value[4096];
	uint32_t size, result;

	/* Start by expecting modern NVRAM area. */
	*old_lockbox = 0;
	size = kLockboxSizeV2;
	result = _read_nvram(value, sizeof(value), kLockboxIndex, size);
	if (result) {
		size = kLockboxSizeV1;
		result = _read_nvram(value, sizeof(value), kLockboxIndex, size);
		if (result) {
			/* No NVRAM area at all. */
			return 0;
		}
		/* Legacy NVRAM area. */
		*old_lockbox = 1;
	}

	debug_dump_hex("nvram", value, size);

	SHA256(value, size, digest);
	debug_dump_hex("digest", digest, DIGEST_LENGTH);

	return 1;
}

int find_system_key(uint8_t *digest, int *migration_allowed)
{
	gchar *key;
	gsize length;

	/* By default, do not allow migration. */
	*migration_allowed = 0;
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
		g_free(key);
		INFO("Using UUID as system key.");
		return 1;
	}

	INFO("Using default insecure system key.");
	sha256("default unsafe static key", digest);
	return 1;
}

char *get_encryption_key(uint8_t *system_key)
{
	char *key = NULL;
	unsigned char *cipher = NULL;
	gsize length;
	uint8_t *plain = NULL;
	int plain_length, final_len;
	EVP_CIPHER_CTX ctx;
	unsigned char digest[DIGEST_LENGTH];
	int i;

	if (g_access(kEncryptedKey, R_OK)) {
		/* This file being missing is handled in caller, so
		 * do not emit error message.
		 */
		INFO("%s does not exist.", kEncryptedKey);
		return 0;
	}

	key = malloc(DIGEST_LENGTH * 2 + 1);
	if (!key) {
		PERROR("malloc");
		goto free_key;
	}

	if (!g_file_get_contents(kEncryptedKey, (gchar **)&cipher, &length,
				 NULL)) {
		PERROR(kEncryptedKey);
		goto free_key;
	}
	plain = malloc(length);
	if (!plain) {
		PERROR("malloc");
		goto free_cipher;
	}

	/* Use the default IV. */
	/* FIXME(keescook): how do I verify that DIGEST_SIZE == key size? */
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

	SHA256(plain, plain_length, digest);

	EVP_CIPHER_CTX_cleanup(&ctx);
	free(plain);
	g_free(cipher);

	for (i = 0; i < DIGEST_LENGTH; ++i)
		sprintf(key + (i * 2), "%02x", digest[i]);
	key[DIGEST_LENGTH * 2] = '\0';

	return key;

free_ctx:
	EVP_CIPHER_CTX_cleanup(&ctx);
free_plain:
	free(plain);
free_cipher:
	g_free(cipher);
free_key:
	free(key);
	return NULL;
}

char *choose_encryption_key(void)
{
	int i;
	char *key;
	unsigned char digest[DIGEST_LENGTH];

	/* FIXME(keescook): Use the TPM instead of OpenSSL's pRNG. */
	if (!RAND_bytes(digest, DIGEST_LENGTH)) {
		SSL_ERROR("RAND_bytes");
		return NULL;
	}

	key = malloc(DIGEST_LENGTH * 2 + 1);
	if (!key) {
		PERROR("malloc");
		return NULL;
	}

	for (i = 0; i < DIGEST_LENGTH; ++i)
		sprintf(key + (i * 2), "%02x", digest[i]);
	key[DIGEST_LENGTH * 2] = '\0';

	return key;
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

int create_bind_src(struct bind_mount *bind)
{
	struct passwd *user;
	struct group *group;

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

int setup_encrypted(void)
{
	uint8_t system_key[DIGEST_LENGTH];
	char *encryption_key = NULL;
	int migrate = 0, rebuild = 0;
	gchar *cmd = NULL;
	char *lodev = NULL, *blksize = NULL;
	struct bind_mount *bind;

	/* Use the "system key" to decrypt the "encryption key" stored in
	 * the stateful partition.
	 */
	if (find_system_key(system_key, &migrate)) {
		encryption_key = get_encryption_key(system_key);
	}
	else {
		INFO("No usable system key found.");
	}

	if (encryption_key)
		migrate = 0;
	else {
		INFO("Generating new encryption key.");
		encryption_key = choose_encryption_key();
		if (!encryption_key)
			return 0;
		rebuild = 1;
	}

	DEBUG("Encryption key is [%s]", encryption_key);

	if (rebuild) {
		int sparse;
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

		INFO("Creating sparse backing file with size %llu.", size);

		/* Create the sparse file. */
		sparse = open(kEncryptedBlock, O_WRONLY | O_CREAT | O_EXCL,
					       S_IRUSR | S_IWUSR);
		if (sparse < 0) {
			PERROR(kEncryptedBlock);
			return 0;
		}
		if (ftruncate(sparse, size)) {
			PERROR("truncate");
			return 0;
		}
		if (close(sparse)) {
			PERROR("close");
			return 0;
		}
	}

	/* Set up loopback device. */
	if (!(cmd = g_strdup_printf("/sbin/losetup -f --show %s",
				    kEncryptedBlock))) {
		PERROR("g_strdup_printf");
		goto failed;
	}
	lodev = read_process_line(cmd);
	if (!lodev || strlen(lodev) == 0) {
		ERROR("losetup failed");
		goto failed;
	}
	g_free(cmd); cmd = NULL;
	INFO("Loopback mounted %s as %s.", kEncryptedBlock, lodev);

	/* Get size as seen by block device. */
	if (!(cmd = g_strdup_printf("/sbin/blockdev --getsz %s", lodev))) {
		PERROR("g_strdup_printf");
		goto losetdown;
	}
	blksize = read_process_line(cmd);
	if (!blksize || strlen(blksize) == 0) {
		ERROR("blockdev failed");
		goto losetdown;
	}
	g_free(cmd); cmd = NULL;

	/* Mount loopback device with dm-crypt using the encryption key. */
	{
		gchar *table = g_strdup_printf("0 %s crypt " \
					       "aes-cbc-essiv:sha256 %s " \
					       "0 %s 0 " \
					       "1 allow_discards",
					       blksize,
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

	if (rebuild) {
		/* Build the ext4 filesystem with "discard" and
		 * "lazy_itable_init" options.
		 */
		const gchar *argv[] = {
			"/sbin/mkfs.ext4",
			"-E", "discard,lazy_itable_init",
			"-m", "0",
			kCryptPath,
			NULL
		};

		INFO("Building filesystem on %s.", kCryptPath);
		if (runcmd(argv) != 0)
			goto dmsetdown;
	}

	/* Mount the dm-crypt partition finally. */
	{
		const char *argv[] = {
			"/bin/mount",
			"-o", "defaults,discard",
			"-t", "ext4",
			kCryptPath, kEncryptedPartition,
			NULL
		};

		INFO("Mounting %s onto %s.", kCryptPath, kEncryptedPartition);
		if (access(kEncryptedPartition, R_OK) &&
		    mkdir(kEncryptedPartition, S_IRWXU | S_IRWXG | \
					       S_IROTH | S_IXOTH)) {
			PERROR(kCryptPath);
			goto dmsetdown;
		}
		if (runcmd(argv) != 0)
			goto dmsetdown;
	}

	// If the lockbox NVRAM area exists buts lacks the new entropy area and /mnt/stateful/var exists:
	if (migrate) {
		/* Migration needs to happen before bind mounting because
		 * some partitions were not already on the stateful partition,
		 * and would over-mounted by the new bind mount.
		 */
		INFO("Want to migrate if possible.");
		// copy the contents of /mnt/stateful/var to /mnt/stateful_partition/encrypted/var
		// if ENOSPC, delete the contents of /mnt/stateful_partition/encrypted/var
		// delete /mnt/stateful/var

		/* FIXME(keescook): actually do this:
		copy old /var
		copy old /home/chronos
		delete old /var
		delete old /home/chronos
		*/
		if (0)
			goto umount;
	}

	/* Perform bind mounts. */
	for (bind = bind_mounts; bind->src; ++ bind) {
		const char *argv[] = {
			"/bin/mount",
			"-o", "bind",
			bind->src, bind->dst,
			NULL
		};

		INFO("Bind mounting %s onto %s.", bind->src, bind->dst);
		if (access(bind->src, R_OK) && create_bind_src(bind))
			goto unbind;
		if (runcmd(argv) != 0)
			goto unbind;
	}

	g_free(cmd);
	free(blksize);
	free(lodev);
	return 1;

unbind:
	for (bind = bind_mounts; bind->src; ++ bind) {
		const char *argv[] = {
			"/bin/umount", bind->dst,
			NULL
		};
		INFO("Unmounting %s.", bind->dst);
		runcmd(argv);
	}
umount:
	{
		const char *argv[] = {
			"/bin/umount", kEncryptedPartition,
			NULL
		};
		INFO("Unmounting %s.", kEncryptedPartition);
		runcmd(argv);
	}
dmsetdown:
	{
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
	{
		const char *argv[] = {
			"/sbin/losetup",
			"-d", lodev,
			NULL
		};
		INFO("Unlooping %s.", lodev);
		runcmd(argv);
	}
failed:
	g_free(cmd);
	free(blksize);
	free(lodev);

	return 0;
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

void sanity_check(void)
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

	/* Verify that bind mounts on stateful haven't happened yet. */
	for (bind = bind_mounts; bind->src; ++ bind) {
		if (!bind->from_stateful)
			continue;

		if (same_vfs(bind->dst, kStatefulPartition)) {
			INFO("%s already bind mounted.", bind->dst)
			exit(1);
		}
	}
}

int main(int argc, char *argv[])
{
	int okay;

	sanity_check();

	okay = setup_encrypted();
	if (!okay) {
		INFO("Setup failed -- clearing files and retrying.");
		unlink(kEncryptedKey);
		unlink(kEncryptedBlock);
		okay = setup_encrypted();
	}

	// Continue boot.
	return okay ? 0 : 1;
}
