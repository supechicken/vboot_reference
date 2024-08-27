/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * This is from the Android Project,
 * Repository: https://android.googlesource.com/platform/system/tools/mkbootimg
 * File: include/bootimg/bootimg.h
 * Commit: cce5b1923e3cd2fcb765b512610bdc5c42bc501d
 *
 * Copyright (C) 2007 The Android Open Source Project
 */

#ifndef _ANDROID_IMAGE_H_
#define _ANDROID_IMAGE_H_
#include <stdint.h>

#define GKI_PAGE_SIZE 4096
#define BOOT_MAGIC "ANDROID!"
#define BOOT_MAGIC_SIZE 8
#define BOOT_NAME_SIZE 16
#define BOOT_ARGS_SIZE 512
#define BOOT_EXTRA_ARGS_SIZE 1024
#define VENDOR_BOOT_MAGIC "VNDRBOOT"
#define VENDOR_BOOT_MAGIC_SIZE 8
#define VENDOR_BOOT_ARGS_SIZE 2048
#define VENDOR_BOOT_NAME_SIZE 16

#define BOOTCONFIG_MAGIC "#BOOTCONFIG\n"
#define BOOTCONFIG_MAGIC_SIZE 12
#define BOOTCONFIG_SIZE_SIZE 4
#define BOOTCONFIG_CHECKSUM_SIZE 4
#define BOOTCONFIG_TRAILER_SIZE (BOOTCONFIG_MAGIC_SIZE + \
				BOOTCONFIG_SIZE_SIZE + \
				BOOTCONFIG_CHECKSUM_SIZE)

struct andr_boot_img_hdr_v3 {
	uint8_t magic[BOOT_MAGIC_SIZE];

	uint32_t kernel_size;    /* size in bytes */
	uint32_t ramdisk_size;   /* size in bytes */

	uint32_t os_version;

	uint32_t header_size;    /* size of boot image header in bytes */
	uint32_t reserved[4];
	uint32_t header_version; /* offset remains constant for version check */

	uint8_t cmdline[BOOT_ARGS_SIZE + BOOT_EXTRA_ARGS_SIZE];
	/* for boot image header v4 only */
	uint32_t signature_size; /* size in bytes */
};

struct andr_vnd_boot_img_hdr {
	uint8_t magic[VENDOR_BOOT_MAGIC_SIZE];
	uint32_t header_version;
	uint32_t page_size;           /* flash page size we assume */

	uint32_t kernel_addr;         /* physical load addr */
	uint32_t ramdisk_addr;        /* physical load addr */

	uint32_t vendor_ramdisk_size; /* size in bytes */

	uint8_t cmdline[VENDOR_BOOT_ARGS_SIZE];

	uint32_t tags_addr;           /* physical addr for kernel tags */

	uint8_t name[VENDOR_BOOT_NAME_SIZE]; /* asciiz product name */
	uint32_t header_size;         /* size of vendor boot image header in bytes */
	uint32_t dtb_size;            /* size of dtb image */
	uint64_t dtb_addr;            /* physical load address */
	/* for boot image header v4 only */
	/* size in bytes for the vendor ramdisk table */
	uint32_t vendor_ramdisk_table_size;
	/* number of entries in the vendor ramdisk table */
	uint32_t vendor_ramdisk_table_entry_num;
	/* size in bytes for a vendor ramdisk table entry */
	uint32_t vendor_ramdisk_table_entry_size;
	uint32_t bootconfig_size; /* size in bytes for the bootconfig section */
};

/* The bootloader expects the structure of andr_boot_img_hdr_v0 with header
 * version 0 to be as follows: */
struct andr_boot_img_hdr_v0 {
	/* Must be BOOT_MAGIC. */
	char magic[BOOT_MAGIC_SIZE];

	uint32_t kernel_size; /* size in bytes */
	uint32_t kernel_addr; /* physical load addr */

	uint32_t ramdisk_size; /* size in bytes */
	uint32_t ramdisk_addr; /* physical load addr */

	uint32_t second_size; /* size in bytes */
	uint32_t second_addr; /* physical load addr */

	uint32_t tags_addr; /* physical addr for kernel tags */
	uint32_t page_size; /* flash page size we assume */

	/* Version of the boot image header. */
	uint32_t header_version;

	/* Operating system version and security patch level.
	 * For version "A.B.C" and patch level "Y-M-D":
	 *   (7 bits for each of A, B, C; 7 bits for (Y-2000), 4 bits for M)
	 *   os_version = A[31:25] B[24:18] C[17:11] (Y-2000)[10:4] M[3:0] */
	uint32_t os_version;

	char name[BOOT_NAME_SIZE]; /* asciiz product name */

	char cmdline[BOOT_ARGS_SIZE];

	uint32_t id[8]; /* timestamp / checksum / sha1 / etc */

	/* Supplemental command line data; kept here to maintain
	 * binary compatibility with older versions of mkbootimg. */
	char extra_cmdline[BOOT_EXTRA_ARGS_SIZE];

	/* Fields in boot_img_hdr_v1 and newer. */
	uint32_t recovery_dtbo_size;   /* size in bytes for recovery DTBO/ACPIO image */
	uint64_t recovery_dtbo_offset; /* offset to recovery dtbo/acpio in boot image */
	uint32_t header_size;

	/* Fields in boot_img_hdr_v2 and newer. */
	uint32_t dtb_size; /* size in bytes for DTB image */
	uint64_t dtb_addr; /* physical load address for DTB image */
} __attribute__((packed));

/* When a boot header is of version 0, the structure of boot image is as
 * follows:
 *
 * +-----------------+
 * | boot header     | 1 page
 * +-----------------+
 * | kernel          | n pages
 * +-----------------+
 * | ramdisk         | m pages
 * +-----------------+
 * | second stage    | o pages
 * +-----------------+
 *
 * n = (kernel_size + page_size - 1) / page_size
 * m = (ramdisk_size + page_size - 1) / page_size
 * o = (second_size + page_size - 1) / page_size
 *
 * 0. all entities are page_size aligned in flash
 * 1. kernel and ramdisk are required (size != 0)
 * 2. second is optional (second_size == 0 -> no second)
 * 3. load each element (kernel, ramdisk, second) at
 *    the specified physical address (kernel_addr, etc)
 * 4. prepare tags at tag_addr.  kernel_args[] is
 *    appended to the kernel commandline in the tags.
 * 5. r0 = 0, r1 = MACHINE_TYPE, r2 = tags_addr
 * 6. if second_size != 0: jump to second_addr
 *    else: jump to kernel_addr
 */

/* When the boot image header has a version of 2, the structure of the boot
 * image is as follows:
 *
 * +---------------------+
 * | boot header         | 1 page
 * +---------------------+
 * | kernel              | n pages
 * +---------------------+
* | ramdisk             | m pages
 * +---------------------+
 * | second stage        | o pages
 * +---------------------+
 * | recovery dtbo/acpio | p pages
 * +---------------------+
 * | dtb                 | q pages
 * +---------------------+
 *
 * n = (kernel_size + page_size - 1) / page_size
 * m = (ramdisk_size + page_size - 1) / page_size
 * o = (second_size + page_size - 1) / page_size
 * p = (recovery_dtbo_size + page_size - 1) / page_size
 * q = (dtb_size + page_size - 1) / page_size
 *
 * 0. all entities are page_size aligned in flash
 * 1. kernel, ramdisk and DTB are required (size != 0)
 * 2. recovery_dtbo/recovery_acpio is required for recovery.img in non-A/B
 *    devices(recovery_dtbo_size != 0)
 * 3. second is optional (second_size == 0 -> no second)
 * 4. load each element (kernel, ramdisk, second, dtb) at
 *    the specified physical address (kernel_addr, etc)
 * 5. If booting to recovery mode in a non-A/B device, extract recovery
 *    dtbo/acpio and apply the correct set of overlays on the base device tree
 *    depending on the hardware/product revision.
 * 6. prepare tags at tag_addr.  kernel_args[] is
 *    appended to the kernel commandline in the tags.
 * 7. r0 = 0, r1 = MACHINE_TYPE, r2 = tags_addr
 * 8. if second_size != 0: jump to second_addr
 *    else: jump to kernel_addr
 */

/* When the boot image header has a version of 3, the structure of the boot
 * image is as follows:
 *
 * +---------------------+
 * | boot header         | 4096 bytes
 * +---------------------+
 * | kernel              | m pages
 * +---------------------+
 * | ramdisk             | n pages
 * +---------------------+
 *
 * m = (kernel_size + 4096 - 1) / 4096
 * n = (ramdisk_size + 4096 - 1) / 4096
 *
 * Note that in version 3 of the boot image header, page size is fixed at 4096 bytes.
 *
 * The structure of the vendor boot image (introduced with version 3 and
 * required to be present when a v3 boot image is used) is as follows:
 *
 * +---------------------+
 * | vendor boot header  | o pages
 * +---------------------+
 * | vendor ramdisk      | p pages
 * +---------------------+
 * | dtb                 | q pages
 * +---------------------+
 * o = (2112 + page_size - 1) / page_size
 * p = (vendor_ramdisk_size + page_size - 1) / page_size
 * q = (dtb_size + page_size - 1) / page_size
 *
 * 0. all entities in the boot image are 4096-byte aligned in flash, all
 *    entities in the vendor boot image are page_size (determined by the vendor
 *    and specified in the vendor boot image header) aligned in flash
 * 1. kernel, ramdisk, vendor ramdisk, and DTB are required (size != 0)
 * 2. load the kernel and DTB at the specified physical address (kernel_addr,
 *    dtb_addr)
 * 3. load the vendor ramdisk at ramdisk_addr
 * 4. load the generic ramdisk immediately following the vendor ramdisk in
 *    memory
 * 5. set up registers for kernel entry as required by your architecture
 * 6. if the platform has a second stage bootloader jump to it (must be
 *    contained outside boot and vendor boot partitions), otherwise
 *    jump to kernel_addr
 */

/* When the boot image header has a version of 4, the structure of the boot
 * image is as follows:
 *
 * +---------------------+
 * | boot header         | 4096 bytes
 * +---------------------+
 * | kernel              | m pages
 * +---------------------+
 * | ramdisk             | n pages
 * +---------------------+
 * | boot signature      | g pages
 * +---------------------+
 *
 * m = (kernel_size + 4096 - 1) / 4096
 * n = (ramdisk_size + 4096 - 1) / 4096
 * g = (signature_size + 4096 - 1) / 4096
 *
 * Note that in version 4 of the boot image header, page size is fixed at 4096
 * bytes.
 *
 * The structure of the vendor boot image version 4, which is required to be
 * present when a version 4 boot image is used, is as follows:
 *
 * +------------------------+
 * | vendor boot header     | o pages
 * +------------------------+
 * | vendor ramdisk section | p pages
 * +------------------------+
 * | dtb                    | q pages
 * +------------------------+
 * | vendor ramdisk table   | r pages
 * +------------------------+
 * | bootconfig             | s pages
 * +------------------------+
 *
 * o = (2128 + page_size - 1) / page_size
 * p = (vendor_ramdisk_size + page_size - 1) / page_size
 * q = (dtb_size + page_size - 1) / page_size
 * r = (vendor_ramdisk_table_size + page_size - 1) / page_size
 * s = (vendor_bootconfig_size + page_size - 1) / page_size
 *
 * Note that in version 4 of the vendor boot image, multiple vendor ramdisks can
 * be included in the vendor boot image. The bootloader can select a subset of
 * ramdisks to load at runtime. To help the bootloader select the ramdisks, each
 * ramdisk is tagged with a type tag and a set of hardware identifiers
 * describing the board, soc or platform that this ramdisk is intended for.
 *
 * The vendor ramdisk section is consist of multiple ramdisk images concatenated
 * one after another, and vendor_ramdisk_size is the size of the section, which
 * is the total size of all the ramdisks included in the vendor boot image.
 *
 * The vendor ramdisk table holds the size, offset, type, name and hardware
 * identifiers of each ramdisk. The type field denotes the type of its content.
 * The vendor ramdisk names are unique. The hardware identifiers are specified
 * in the board_id field in each table entry. The board_id field is consist of a
 * vector of unsigned integer words, and the encoding scheme is defined by the
 * hardware vendor.
 *
 * For the different type of ramdisks, there are:
 *    - VENDOR_RAMDISK_TYPE_NONE indicates the value is unspecified.
 *    - VENDOR_RAMDISK_TYPE_PLATFORM ramdisks contain platform specific bits, so
 *      the bootloader should always load these into memory.
 *    - VENDOR_RAMDISK_TYPE_RECOVERY ramdisks contain recovery resources, so
 *      the bootloader should load these when booting into recovery.
 *    - VENDOR_RAMDISK_TYPE_DLKM ramdisks contain dynamic loadable kernel
 *      modules.
 *
 * Version 4 of the vendor boot image also adds a bootconfig section to the end
 * of the image. This section contains Boot Configuration parameters known at
 * build time. The bootloader is responsible for placing this section directly
 * after the generic ramdisk, followed by the bootconfig trailer, before
 * entering the kernel.
 *
 * 0. all entities in the boot image are 4096-byte aligned in flash, all
 *    entities in the vendor boot image are page_size (determined by the vendor
 *    and specified in the vendor boot image header) aligned in flash
 * 1. kernel, ramdisk, and DTB are required (size != 0)
 * 2. load the kernel and DTB at the specified physical address (kernel_addr,
 *    dtb_addr)
 * 3. load the vendor ramdisks at ramdisk_addr
 * 4. load the generic ramdisk immediately following the vendor ramdisk in
 *    memory
 * 5. load the bootconfig immediately following the generic ramdisk. Add
 *    additional bootconfig parameters followed by the bootconfig trailer.
 * 6. set up registers for kernel entry as required by your architecture
 * 7. if the platform has a second stage bootloader jump to it (must be
 *    contained outside boot and vendor boot partitions), otherwise
 *    jump to kernel_addr
 */
struct boot_img_hdr_v4 {
	uint8_t magic[BOOT_MAGIC_SIZE];

	uint32_t kernel_size;    /* size in bytes */
	uint32_t ramdisk_size;   /* size in bytes */

	uint32_t os_version;

	uint32_t header_size;    /* size of boot image header in bytes */
	uint32_t reserved[4];
	uint32_t header_version; /* offset remains constant for version check */

	uint8_t cmdline[BOOT_ARGS_SIZE + BOOT_EXTRA_ARGS_SIZE];
	/* for boot image header v4 only */
	uint32_t signature_size; /* size in bytes */
} __attribute__((packed));

struct vendor_boot_img_hdr_v4 {
	uint8_t magic[VENDOR_BOOT_MAGIC_SIZE];

	uint32_t header_version;
	uint32_t page_size;
	uint32_t kernel_addr;
	uint32_t ramdisk_addr;
	uint32_t vendor_ramdisk_size;   /* size in bytes */

	uint8_t cmdline[VENDOR_BOOT_ARGS_SIZE];

	uint32_t tags_addr;
	uint8_t name[VENDOR_BOOT_NAME_SIZE];
	uint32_t header_size;

	uint32_t dtb_size;
	uint64_t dtb_addr;

	uint32_t vendor_ramdisk_table_size;
	uint32_t vendor_ramdisk_table_entry_num;
	uint32_t vendor_ramdisk_table_entry_size;
	uint32_t bootconfig_size;
} __attribute__((packed));
#endif
