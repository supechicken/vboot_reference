/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef VBOOT_REFERENCE_CHROMEOS_CONFIG_H
#define VBOOT_REFERENCE_CHROMEOS_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#include "2common.h"
#include "2return_codes.h"

typedef vb2_error_t (*read_file_fcn_t)(const char *filepath, uint8_t **data_ptr,
				       uint32_t *size_ptr);

/**
 * Setup a function which mocks vb2_read_file.
 *
 * @param mock_function    A function which mocks vb2_read_file.
 */
void chromeos_config_mock_read_file_for_testing(read_file_fcn_t mock_function);

/**
 * Get a value from the model configuration on the device as a string.
 *
 * Note: this function allocates memory by its use of vb2_read_file, and relies
 * on the caller to free the allocated memory. The output parameter will be set
 * to NULL upon failure, so free can be safely called on this parameter.
 *
 * @param path          The path in the config schema to the object containing
 *                      the requested property.
 * @param property      The name of the requested property.
 * @param val_out       Output parameter which gets assigned to a
 *                      null-terminated string.
 * @return VB2_SUCCESS on success, or a relevant error upon error.
 */
vb2_error_t chromeos_config_get_string(const char *path, const char *property,
				       char **val_out);

/**
 * Get a value from the model configuration on the device as a boolean.
 *
 * @param path          The path in the config schema to the object containing
 *                      the requested property.
 * @param property      The name of the requested property.
 * @param val_out       Output parameter which gets assigned to a boolean.
 * @return VB2_SUCCESS on success, or a relevant error upon error.
 */
vb2_error_t chromeos_config_get_boolean(const char *path, const char *property,
					bool *val_out);

/**
 * Get a value from the model configuration on the device as an integer.
 *
 * @param path          The path in the config schema to the object containing
 *                      the requested property.
 * @param property      The name of the requested property.
 * @param val_out       Output parameter which gets assigned to an integer.
 * @return VB2_SUCCESS on success, or a relevant error upon error.
 */
vb2_error_t chromeos_config_get_integer(const char *path, const char *property,
					int *val_out);

#endif /* VBOOT_REFERENCE_CHROMEOS_CONFIG_H */
