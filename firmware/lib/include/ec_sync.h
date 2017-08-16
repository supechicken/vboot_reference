/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * EC software sync for verified boot
 */

#ifndef VBOOT_REFERENCE_EC_SYNC_H_
#define VBOOT_REFERENCE_EC_SYNC_H_

#include "vboot_api.h"

struct vb2_context;
struct VbCommonParams;

/* Supported features */
enum ec_feature_code {
	/*
	 * This image contains a limited set of features. Another image
	 * in RW partition may support more features.
	 */
	EC_FEATURE_LIMITED = 0,
	/*
	 * Commands for probing/reading/writing/erasing the flash in the
	 * EC are present.
	 */
	EC_FEATURE_FLASH = 1,
	/*
	 * Can control the fan speed directly.
	 */
	EC_FEATURE_PWM_FAN = 2,
	/*
	 * Can control the intensity of the keyboard backlight.
	 */
	EC_FEATURE_PWM_KEYB = 3,
	/*
	 * Support Google lightbar, introduced on Pixel.
	 */
	EC_FEATURE_LIGHTBAR = 4,
	/* Control of LEDs  */
	EC_FEATURE_LED = 5,
	/* Exposes an interface to control gyro and sensors.
	 * The host goes through the EC to access these sensors.
	 * In addition, the EC may provide composite sensors, like lid angle.
	 */
	EC_FEATURE_MOTION_SENSE = 6,
	/* The keyboard is controlled by the EC */
	EC_FEATURE_KEYB = 7,
	/* The AP can use part of the EC flash as persistent storage. */
	EC_FEATURE_PSTORE = 8,
	/* The EC monitors BIOS port 80h, and can return POST codes. */
	EC_FEATURE_PORT80 = 9,
	/*
	 * Thermal management: include TMP specific commands.
	 * Higher level than direct fan control.
	 */
	EC_FEATURE_THERMAL = 10,
	/* Can switch the screen backlight on/off */
	EC_FEATURE_BKLIGHT_SWITCH = 11,
	/* Can switch the wifi module on/off */
	EC_FEATURE_WIFI_SWITCH = 12,
	/* Monitor host events, through for example SMI or SCI */
	EC_FEATURE_HOST_EVENTS = 13,
	/* The EC exposes GPIO commands to control/monitor connected devices. */
	EC_FEATURE_GPIO = 14,
	/* The EC can send i2c messages to downstream devices. */
	EC_FEATURE_I2C = 15,
	/* Command to control charger are included */
	EC_FEATURE_CHARGER = 16,
	/* Simple battery support. */
	EC_FEATURE_BATTERY = 17,
	/*
	 * Support Smart battery protocol
	 * (Common Smart Battery System Interface Specification)
	 */
	EC_FEATURE_SMART_BATTERY = 18,
	/* EC can detect when the host hangs. */
	EC_FEATURE_HANG_DETECT = 19,
	/* Report power information, for pit only */
	EC_FEATURE_PMU = 20,
	/* Another Cros EC device is present downstream of this one */
	EC_FEATURE_SUB_MCU = 21,
	/* Support USB Power delivery (PD) commands */
	EC_FEATURE_USB_PD = 22,
	/* Control USB multiplexer, for audio through USB port for instance. */
	EC_FEATURE_USB_MUX = 23,
	/* Motion Sensor code has an internal software FIFO */
	EC_FEATURE_MOTION_SENSE_FIFO = 24,
	/* Support temporary secure vstore */
	EC_FEATURE_VSTORE = 25,
	/* EC decides on USB-C SS mux state, muxes configured by host */
	EC_FEATURE_USBC_SS_MUX_VIRTUAL = 26,
	/* EC has RTC feature that can be controlled by host commands */
	EC_FEATURE_RTC = 27,
	/* EC has slot RW-A and RW-B */
	EC_FEATURE_RW_AB = 32,
};

#define EC_FEATURE_MASK_0(event_code) (1UL << (event_code % 32))
#define EC_FEATURE_MASK_1(event_code) (1UL << (event_code - 32))
/**
 * EC sync, phase 1
 *
 * This checks whether the EC is running the correct image to do EC sync, and
 * whether any updates are necessary.
 *
 * @param ctx		Vboot2 context
 * @param cparams	Vboot common params
 * @return VBERROR_SUCCESS, VBERROR_EC_REBOOT_TO_RO_REQUIRED if the EC must
 * reboot back to its RO code to continue EC sync, or other non-zero error
 * code.
 */
VbError_t ec_sync_phase1(struct vb2_context *ctx,
			 struct VbCommonParams *cparams);

/**
 * Returns non-zero if the EC will perform a slow update during phase 2.
 *
 * This is only valid after calling ec_sync_phase1(), before calling
 * ec_sync_phase2().
 *
 * @param ctx		Vboot2 context
 * @param cparams	Vboot common params
 * @return non-zero if a slow update will be done; zero if no update or a
 * fast update.
 */
int ec_will_update_slowly(struct vb2_context *ctx,
			  struct VbCommonParams *cparams);

/**
 * Check if auxiliary firmware blobs need to be updated.
 *
 * @param ctx		Vboot2 context
 * @param cparams	Vboot common params
 * @param severity	VB_AUX_FW_{NO,FAST,SLOW}_UPDATE
 * @return VBERROR_SUCCESS or non-zero error code.
 */
VbError_t ec_sync_check_aux_fw(struct vb2_context *ctx,
			       struct VbCommonParams *cparams,
			       VbAuxFwUpdateSeverity_t *severity);

/**
 * EC sync, phase 2
 *
 * This updates the EC if necessary, makes sure it has protected its image(s),
 * and makes sure it has jumped to the correct image.
 *
 * If ec_will_update_slowly(), it is suggested that the caller display a
 * warning screen before calling phase 2.
 *
 * @param ctx		Vboot2 context
 * @param cparams	Vboot common params
 * @return VBERROR_SUCCESS, VBERROR_EC_REBOOT_TO_RO_REQUIRED if the EC must
 * reboot back to its RO code to continue EC sync, or other non-zero error
 * code.
 */
VbError_t ec_sync_phase2(struct vb2_context *ctx,
			 struct VbCommonParams *cparams);

/**
 * EC sync, phase 3
 *
 * This completes EC sync and handles battery cutoff if needed.
 *
 * @param ctx		Vboot2 context
 * @param cparams	Vboot common params
 * @return VBERROR_SUCCESS or non-zero error code.
 */
VbError_t ec_sync_phase3(struct vb2_context *ctx, VbCommonParams *cparams);

/**
 * Sync all EC devices to expected versions.
 *
 * This is a high-level function which calls the functions above.
 *
 * @param ctx		Vboot context
 * @param devidx	EC device index to sync
 * @param cparams	Common vboot params
 * @return VBERROR_SUCCESS, or non-zero if error.
 */
VbError_t ec_sync_all(struct vb2_context *ctx, struct
		      VbCommonParams *cparams);

#endif  /* VBOOT_REFERENCE_EC_SYNC_H_ */
