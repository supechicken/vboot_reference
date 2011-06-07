/* Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* APIs provided by firmware to vboot_reference. */

/* General notes:
 *
 * All verified boot functions now start with "Vb" for namespace
 * clarity.  This fixes the problem where uboot and vboot both defined
 * assert().
 *
 * Verified boot APIs to be implemented by the calling firmware and
 * exported to vboot_reference start with "VbEx".
 */
/* TODO: split this file into a vboot_entry_points.h file which
 * contains the entry points for the firmware to call vboot_reference,
 * and a vboot_firmware_exports.h which contains the APIs to be
 * implemented by the calling firmware and exported to
 * vboot_reference. */

#ifndef VBOOT_REFERENCE_VBOOT_WRAPPER_H_
#define VBOOT_REFERENCE_VBOOT_WRAPPER_H_

#include "sysincludes.h"

/*****************************************************************************/
/* Error codes */

/* Functions which return error all return this type.  This is a
* 32-bit value rather than an int so it's consistent across UEFI,
* which is 32-bit during PEI and 64-bit during DXE/BDS. */
typedef uint32_t VbError_t;

/* No error; function completed successfully. */
#define VBERROR_SUCCESS 0
/* Unknown/unspecified error. */
#define VBERROR_FAILURE 1
/* Function not implemented. */
#define VBERROR_UNIMPLEMENTED 2


/*****************************************************************************/
/* Debug output (from utility.h) */

/* Output an error message and quit.  Does not return.  Supports
 * printf()-style formatting. */
void VbExError(const char* format, ...);

/* Output a debug message.  Supports printf()-style formatting. */
void VbExDebug(const char* format, ...);


/*****************************************************************************/
/* Memory (from utility.h) */

/* Allocate [size] bytes and return a pointer to the allocated memory. Abort
 * on error; this always either returns a good pointer or never returns.
 *
 * If any of the firmware API implementations require aligned data
 * (for example, disk access on ARM), all pointers returned by
 * VbExMalloc() must also be aligned.  */
void* VbExMalloc(size_t size);

/* Free memory pointed by [ptr] previously allocated by Malloc(). */
void VbExFree(void* ptr);


/*****************************************************************************/
/* Timer and delay (first two from utility.h) */

/* Read a high-resolution timer.
 *
 * This is intended for benchmarking, so this call MUST be fast.  The
 * timer frequency must be >1 KHz (preferably >1 MHz), and the timer
 * must not wrap around for at least 10 minutes.  It is preferable
 * (but not required) that the timer be initialized to 0 at boot.
 *
 * It is assumed that the firmware has some other way of communicating
 * the timer frequency to the OS.  For example, on x86 we use TSC, and
 * the OS kernel reports the initial TSC value at kernel-start and
 * calculates the frequency. */
uint64_t VbExGetTimer(void);

/* Delay for at least the specified number of milliseconds.  Should be
 * accurate to within 10% (a requested delay of 1000 ms should
 * result in an actual delay of between 1000 - 1100 ms). */
void VbExSleepMs(uint32_t msec);

/* Play a beep tone of the specified frequency in Hz and duration in msec.
 * This is effectively a VbSleep() variant that makes noise.
 *
 * The implementation should do the best it can if it cannot fully
 * support this interface - for example, beeping at a fixed frequency
 * if frequency support is not available.  At a minimum, it must delay for
 * the specified duration. */
void VbExBeep(uint32_t msec, uint32_t frequency);


/*****************************************************************************/
/* TPM (from tlcl_stub.h) */

/* Initialize the stub library. */
VbError_t VbExTpmInit(void);

/* Close and open the device.  This is needed for running more complex commands
 * at user level, such as TPM_TakeOwnership, since the TPM device can be opened
 * only by one process at a time. */
VbError_t VbExTpmClose(void);
VbError_t VbExTpmOpen(void);

/* Send a request_length-byte request to the TPM and receive a
 * response.  On input, response_length is the size of the response
 * buffer in bytes.  On exit, response_length is set to the actual
 * received response length in bytes. */
VbError_t VbExTpmSendReceive(const uint8_t* request, uint32_t request_length,
                             uint8_t* response, uint32_t* response_length);

/* NOTE: We've pondered splitting apart SendReceive() to allow the TPM
 * to work asynchronously; this will speed boot time.  So we'd have
 * Send(), and then both blocking and non-blocking Receive().  Not
 * going to do that now. */


/*****************************************************************************/
/* Firmware / EEPROM access (previously in load_firmware_fw.h) */

/* Firmware types for VbHashFirmwareBody() and
 * VbSelectFirmwareParams.selected_firmware.  Note that we store these
 * in a uint32_t because enum maps to int, which isn't fixed-size. */
enum VbSelectFirmware_t {
  VB_SELECT_FIRMWARE_RECOVERY = 0,
  VB_SELECT_FIRMWARE_A = 1,
  VB_SELECT_FIRMWARE_B = 2
};

/* Read the firmware body data for [firmware_index], which is either
 * VB_SELECT_FIRMWARE_A or VB_SELECT_FIRMWARE B.
 *
 * This function must call VbUpdateFirmwareBodyHash() before returning,
 * to update the secure hash for the firmware image.  For best
 * performance, the reader should call this function periodically
 * during the read, so that updating the hash can be pipelined with
 * the read.  If the reader cannot update the hash during the read
 * process, it should call VbUpdateFirmwareBodyHash() on the entire
 * firmeware data after the read, before returning.
 *
 * It is recommended that the firmware use this call to copy the
 * requested firmware body from EEPROM into RAM, so that it doesn't
 * need to do a second slow copy from EEPROM to RAM if this firmware
 * body is selected. */
VbError_t VbExHashFirmwareBody(VbSelectFirmwareParams* params,
                               uint32_t firmware_index);
/* NOTE: That function doesn't actually pass the firmware body to
 * vboot, because vboot doesn't need it - just its hash.  This is
 * important on x86, where the firmware is stored compressed.  We hash
 * the compressed data, but the BIOS decompresses it during read.
 * Simply updating a hash is compatible with the x86
 * read-and-decompress pipeline. */

/* Functions provided by verified boot library to firmware, shown here
 * for reference. */
/* Update the data hash for the current firmware image, extending it
 * by [size] bytes stored in [*data].  This function must only be
 * called inside VbGetFirmwareBody(). */
void VbUpdateFirmwareBodyHash(VbSelectFirmwareParams* params,
                              uint8_t* data, uint32_t size);

/*****************************************************************************/
/* Disk access (previously in boot_device.h) */

/* We use disk handles rather than indices.  Using indices causes problems if
 * a disk is removed/inserted in the middle of processing. */
typedef void* VbExDiskHandle_t;

/* Flags for VbDisk APIs */
/* Disk is removable.  Example removable disks: SD cards, USB keys.  */
#define VB_DISK_FLAG_REMOVABLE 0x00000001
/* Disk is fixed.  If this flag is present, disk is internal to the
 * system and not removable.  Example fixed disks: internal SATA SSD, eMMC. */
#define VB_DISK_FLAG_FIXED     0x00000002
/* Note that VB_DISK_FLAG_REMOVABLE and VB_DISK_FLAG_FIXED are
 * mutually-exclusive for a single disk.  VbExGetDiskHandles() may specify
 * both flags to request disks of both types in a single call. */
/* At some point we could specify additional flags, but we don't currently
 * have a way to make use of these:
 *
 * USB              Device is known to be attached to USB.  Note that the SD
 *                  card reader inside x86 systems is attached to USB so this
 *                  isn't super useful.
 * SD               Device is known to be a SD card.  Note that external card
 *                  readers might not return this information, so also of
 *                  questionable use.
 * READ_ONLY        Device is known to be read-only.  Could be used by recovery
 *                  when processing read-only recovery image.
 **/

/* Information on a single disk */
typedef struct VbDiskInfo {
  VbExDiskHandle_t handle;  /* Disk handle */
  uint64_t bytes_per_lba;   /* Size of a LBA sector in bytes */
  uint64_t lba_count;       /* Number of LBA sectors on the device */
  uint32_t flags;           /* Flags (see VB_DISK_FLAG_* constants) */
  const char* name;         /* Optional name string, for use in debugging.
                             * May be empty or null if not available. */
} VbDiskInfo;

/* Store information into [info] for all disks (storage devices)
 * attached to the system which match all of the disk_flags.
 *
 * On input, count is set to the size of the info array.  On output, it is
 * set to the actual number of matching disks found.
 *
 * A multi-function device (such as a 4-in-1 card reader) should provide
 * multiple disk handles.
 *
 * If the firmware dynamically allocates handles, it must track and
 * freem them after VbSelectKernel() returns.  The handles can't be freed by
 * VbSelectKernel() itself, because it needs to return the handle for
 * the selected disk.
 *
 * Handles from one call to VbExGetDiskInfo() must remain usable in
 * VbExDiskRead() and VbExDiskWrite() functions until either
 * VbExDiskInfo() is called again (returning new disk information with
 * potentially new handles) or VbSelectKernel() returns.  Using a
 * handle associated with a now-removed disk may cause the read/write
 * functions to return error, but it shouldn't cause them to access
 * now-freed memory. */
VbError_t VbExGetDiskInfo(VbDiskInfo* info, uint32 count, uint32_t disk_flags);

/* TODO: change to us passing in a list of info structs and them
 * populating it.  Each info struct has a handle pointer.  No **, and
 * it's clear who owns that memory.
 *
 * Ideally, handles aren't reused between calls if the disk is removed
 * and another disk is inserted into the same plug.
 *
 * The guarantee we need is that if a disk is removed, and we pass a
 * handle from that into read/write, it doesn't crash.  All handles
 * returned by get-info need to remain valid until the next call to
 * get-info.
 *
 * Security-wise we're ok if someone swaps disks while we're
 * validating the kernel, since we load into RAM and then validate the
 * RAM at each stage. */


/* Read lba_count LBA sectors, starting at sector lba_start, from the disk,
 * into the buffer.  Assumes the buffer is at least cache line aligned; use
 * VbExAlignedMalloc() instead of VbExMalloc() to allocate the buffer. */
VbError_t VbExDiskRead(VbExDiskHandle_t handle, uint64_t lba_start,
                       uint64_t lba_count, void* buffer);


/* Write lba_count LBA sectors, starting at sector lba_start, to the
 * disk, from the buffer.  Assumes the buffer is at least cache line
 * aligned; use VbExAlignedMalloc() instead of VbExMalloc() to
 * allocate the buffer. */
VbError_t VbExDiskWrite(VbExDiskHandle_t handle, uint64_t lba_start,
                        uint64_t lba_count, const void* buffer);


/*****************************************************************************/
/* Display */

/* Initialize and clear the display.  Set width and height to the screen
 * dimensions in pixels. */
VbError_t VbExDisplayInit(uint32_t* width, uint32_t* height);


/* Write a bitmap to the display, with the upper left corner at the
 * specified pixel coordinates.  The bitmap buffer is a
 * platform-dependent binary blob with the specified size in bytes. */
VbError_t VbExDisplayBitmap(uint32_t x, uint32_t y, const void* buffer,
                            uint32_t buffer_size);


/* Display a string containing debug information on the screen,
 * rendered in a platform-dependent font.  Should be able to handle
 * newlines '\n' in the string.  Firmware must support displaying at
 * least 20 lines of text, where each line may be at least 80
 * characters long.  If the firmware has its own debug state, it may
 * display it to the screen below this information. */
VbError_t VbExDisplayDebugInfo(const char* info_str);
/* NOTE: This is what we currently display on ZGB/Alex when TAB is
 * pressed.  Some information (HWID, recovery reason) is ours; some
 * (CMOS breadcrumbs) is platform-specific.  If we decide to
 * soft-render the HWID string, we'll need to maintain our own fonts,
 * so will likely display it via VbDisplayBitmap() above. */


/*****************************************************************************/
/* Keyboard */

/* Key codes for required non-printable-ASCII characters. */
enum VbKeyCode_t {
  VB_KEY_UP = 0x100,
  VB_KEY_DOWN = 0x101,
  VB_KEY_LEFT = 0x102,
  VB_KEY_RIGHT = 0x103
};

/* Read the next keypress from the keyboard buffer.
 *
 * Returns the keypress, or zero if no keypress is pending or error.
 *
 * The following keys must be returned as ASCII character codes:
 *    0x08          Backspace
 *    0x09          Tab
 *    0x0D          Enter (carriage return)
 *    0x01 - 0x1A   Ctrl+A - Ctrl+Z (yes, those alias with backspace/tab/enter)
 *    0x1B          Esc
 *    0x20          Space
 *    0x30 - 0x39   '0' - '9'
 *    0x60 - 0x7A   'a' - 'z'
 *
 * Some extended keys must also be supported; see the VB_KEY_* defines above.
 *
 * Keys ('/') or key-chords (Fn+Q) not defined above may be handled in any of
 * the following ways:
 *    1. Filter (don't report anything if one of these keys is pressed).
 *    2. Report as ASCII (if a well-defined ASCII value exists for the key).
 *    3. Report as any other value in the range 0x200 - 0x2FF.
 * It is not permitted to report a key as a multi-byte code (for example,
 * sending an arrow key as the sequence of keys '\x1b', '[', '1', 'A'). */
uint32_t VbExReadKeyboard(void);


/*****************************************************************************/
/* Misc */

/* Checks if the firmware needs to shut down the system.
 *
 * Returns 1 if a shutdown is being requested (for example, the user has
 * pressed the power button or closed the lid), or 0 if a shutdown is not
 * being requested. */
/* NOTE: When we're displaying a screen, pressing the power button
 * should shut down the computer.  We need a way to break out of our
 * control loop so this can occur cleanly. */
uint32_t VbExShutdownRequested(void);

/*****************************************************************************/
/* Main entry points */

/* Data passed by firmware to both VbSelectFirmware() and VbSelectKernel(). */
/* Note that in UEFI, these are called by different phases in
 * different processor modes (VbSelectFirmware() = 32-bit PEI,
 * VbSelectKernel() = 64-bit BDS), so the data may be at a different
 * location between calls. */
typedef struct VbSelectData {
  void* gbb_data;                /* Pointer to GBB data */
  uint32_t gbb_size;             /* Size of GBB data in bytes */

  /* Shared data blob for data shared between VbSelectFirmware() and
   * VbSelectKernel().  This should be at least VB_SHARED_DATA_MIN_SIZE bytes
   * long, and ideally is VB_SHARED_DATA_REC_SIZE bytes long. */
  void* shared_data_blob;        /* Shared data blob buffer. */
  uint32_t shared_data_size;     /* On input, set to size of shared data blob
                                  * buffer, in bytes.  On output, this will
                                  * contain the actual data size placed into
                                  * the buffer. */

  uint32_t flags;                /* Flags; see VBSD_FLAG_*. */
  VbNvContext* nv_context;       /* Context for NV storage.
                                  * nv_context->raw must be filled
                                  * before calling VbSelectFirmware()
                                  * / VbSelectKernel().  On output,
                                  * check nv_context->raw_changed to
                                  * see if nv_context->raw has been
                                  * modified and needs saving. */
} VbSelectData;


/* Flags for VbSelectFirmware.flags */
/* Developer switch was on at boot time. */
#define VBSF_FLAG_DEV_SWITCH_ON       0x00000001
/* Recovery button was pressed at boot time. */
#define VBSF_FLAG_REC_BUTTON_PRESSED  0x00000002
/* Hardware write protect was enabled at boot time. */
#define VBSF_FLAG_WP_ENABLED          0x00000004


/* Data only used by VbSelectFirmware() */
typedef struct VbSelectFirmwareParams {
  /* Inputs to VbSelectFirmware() */
  VbSelectParams common_params;  /* Parameters provided by firmware to
                                  * both firmware and kernel selection */
  uint32_t flags;                /* Flags (see VBSF_FLAG_*) */
  void* verification_block_A;    /* Key block + preamble for firmware A */
  void* verification_block_B;    /* Key block + preamble for firmware B */
  uint32_t verification_size_A;  /* Verification block A size in bytes */
  uint32_t verification_size_B;  /* Verification block B size in bytes */

  /* Outputs from VbSelectFirmware(); valid only if it returns success. */
  uint32_t selected_firmware;    /* Main firmware to run; see VB_SELECT_*. */

  /* Internal context/data for verified boot, to maintain state during
   * calls to other API functions such as VbExHashFirmwareBody().
   * Allocated and freed inside VbSelectFirmware(); firmware should
   * not look at this. */
  void* vboot_context;

  /* Internal context/data for firmware / VbExHashFirmwareBody().
   * Needed because the PEI phase of UEFI boot doesn't have global
   * variables, so everything gets passed around on the stack. */
  void* caller_context;

} VbSelectFirmwareParams;


/* Data used only by VbSelectKernel() */
typedef struct VbSelectKernelParams {
  /* Inputs to VbSelectKernel() */
  VbSelectParams common_params;  /* Parameters provided by firmware to
                                  * both firmware and kernel selection */
  void* kernel_buffer;           /* Destination buffer for kernel
                                  * (normally at 0x100000 on x86) */
  uint32_t kernel_buffer_size;   /* Size of kernel buffer in bytes */

  /* Outputs from VbSelectKernel(); valid only if it returns success. */
  VbExDiskHandle_t disk_handle;  /* Handle of disk containing loaded kernel */
  uint32_t partition_number;     /* Partition number on disk to boot (1...M) */
  uint64_t bootloader_address;   /* Address of bootloader image in RAM */
  uint32_t bootloader_size;      /* Size of bootloader image in bytes */
  uint8_t partition_guid[16];    /* UniquePartitionGuid for boot partition */
  /* TODO: in H2C, all that pretty much just gets passed to the bootloader
   * as KernelBootloaderOptions, though the disk handle is passed as an index
   * instead of a handle.  Is that used anymore now that we're passing
   * partition_guid? */
} VbSelectKernelParams;

/* Select the main firmware.
 *
 * Returns 0 if success, non-zero if error; on error, caller should reboot. */
/* NOTE: This is now called in all modes, including recovery.
 * Previously, LoadFirmware() was not called in recovery mode, which
 * meant that LoadKernel() needed to duplicate the TPM and
 * VbSharedData initialization code. */
VbError_t VbSelectFirmware(VbSelectFirmwareParams* params);

/* Select and loads the kernel.
 *
 * Returns 0 if success, non-zero if error; on error, caller should reboot. */
VbError_t VbSelectKernel(VbSelectKernelParams* params);

/* S3 resume handler.  This only needs to be called if the hardware
 * does not maintain power to the TPM when in S3 (suspend-to-RAM).
 *
 * Returns 0 if success, non-zero if error; on error, caller should reboot. */
VbError_t VbS3Resume(void);


#endif  /* VBOOT_REFERENCE_VBOOT_WRAPPER_H_ */
