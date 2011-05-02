/* Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "vboot_common.h"
#include "vboot_nvstorage.h"
#include "host_common.h"

#define CONFIG_LENGTH_FMAP 0x400

#define offsetof(struct_name, field) ((int) &(((struct_name*)0)->field))
/*
 * This structure has been copied from u-boot-next
 * files/lib/chromeos/os_storage.c. Keep it in sync until a better interface
 * is implemented.
 */
typedef struct {
  uint32_t total_size;
  uint8_t  signature[12];
  uint64_t nvcxt_lba;
  uint16_t vbnv[2];
  uint8_t  nvcxt_cache[VBNV_BLOCK_SIZE];
  uint8_t  write_protect_sw;
  uint8_t  recovery_sw;
  uint8_t  developer_sw;
  uint8_t  binf[5];
  uint32_t chsw;
  uint8_t  hwid[256];
  uint8_t  fwid[256];
  uint8_t  frid[256];
  uint32_t fmap_base;
  uint8_t  shared_data_body[CONFIG_LENGTH_FMAP];
} __attribute__((packed)) VbSharedMem;

static VbSharedMem shared_memory;

typedef struct {
  const char *cs_field_name;
  const void *cs_value;
} VbVarInfo;

static VbVarInfo vb_cs_map[] = {
  {"hwid", &shared_memory.hwid},
  {"fwid", &shared_memory.fwid},
  {"ro_fwid", &shared_memory.frid},
  {"devsw_boot", &shared_memory.developer_sw},
  {"recoverysw_boot", &shared_memory.recovery_sw},
  {"wpsw_boot", &shared_memory.write_protect_sw},
};

static int VbReadSharedMemory(void)
{
  const char *blob_name = "/sys/kernel/debug/chromeos_arm";
  FILE *data_file = NULL;
  int rv = -1;
  int size;

  do {
    data_file = fopen(blob_name, "rb");
    if (!data_file) {
      fprintf(stderr, "%s: failed to open %s\n", __FUNCTION__, blob_name);
      break;
    }
    size = fread(&shared_memory, 1, sizeof(shared_memory), data_file);
    if ((size != sizeof(shared_memory)) || (size != shared_memory.total_size)) {
      fprintf(stderr,  "%s: failed to read shared memory: got %d bytes, "
	      "expected %d, should have been %d\n",
	      __FUNCTION__,
	      size,
	      sizeof(shared_memory),
	      shared_memory.total_size);
      break;
    }
    rv = 0;
  } while (0);

  if (data_file)
    fclose(data_file);

  return rv;
}

static const void* VbGetVarAuto(const char* name)
{
  int i;
  VbVarInfo *pi;

  for (i = 0, pi = vb_cs_map; i < ARRAY_SIZE(vb_cs_map); i++, pi++) {
    if (strcmp(pi->cs_field_name, name)) continue;
    return pi->cs_value;
  }
  return NULL;
}

int VbReadNvStorage(VbNvContext* vnc) {
  Memcpy(vnc->raw, shared_memory.nvcxt_cache, sizeof(vnc->raw));
  return 0;
}


int VbWriteNvStorage(VbNvContext* vnc) {
  /* TODO: IMPLEMENT ME! */
  return -1;
}


VbSharedDataHeader* VbSharedDataRead(void) {
  /* TODO: IMPLEMENT ME! */
  return NULL;
}


int VbGetArchPropertyInt(const char* name) {

  const uint8_t *value;

  value = VbGetVarAuto(name);

  if (value) return (int) *value;

  /* TODO: IMPLEMENT ME!  For now, return reasonable defaults for
   * values where reasonable defaults exist. */
  if (!strcasecmp(name,"recovery_reason")) {
  } else if (!strcasecmp(name,"fmap_base")) {
    return shared_memory.fmap_base;
  }
  /* Switch positions */
  else if (!strcasecmp(name,"devsw_cur")) {
    return 1;
  } else if (!strcasecmp(name,"recoverysw_cur")) {
    return 0;
  } else if (!strcasecmp(name,"wpsw_cur")) {
    return 1;
  } else if (!strcasecmp(name,"recoverysw_ec_boot")) {
    return 0;
  }

  /* Saved memory is at a fixed location for all H2C BIOS.  If the CHSW
   * path exists in sysfs, it's a H2C BIOS. */
  else if (!strcasecmp(name,"savedmem_base")) {
  } else if (!strcasecmp(name,"savedmem_size")) {
  }

  return -1;
}


const char* VbGetArchPropertyString(const char* name, char* dest, int size) {
  const char* value = VbGetVarAuto(name);
  if (value) return StrCopy(dest, value, size);

  /* TODO: IMPLEMENT ME!  For now, return reasonable defaults for
   * values where reasonable defaults exist. */
  if (!strcasecmp(name,"arch")) {
    return StrCopy(dest, "arm", size);
  } else if (!strcasecmp(name,"mainfw_act")) {
    return StrCopy(dest, "A", size);
  } else if (!strcasecmp(name,"mainfw_type")) {
    return StrCopy(dest, "developer", size);
  } else if (!strcasecmp(name,"ecfw_act")) {
    return StrCopy(dest, "RO", size);
  }

  return NULL;
}


int VbSetArchPropertyInt(const char* name, int value) {
  /* TODO: IMPLEMENT ME! */
  return -1;
}


int VbSetArchPropertyString(const char* name, const char* value) {
  /* If there were settable architecture-dependent string properties,
   * they'd be here. */
  return -1;
}

int VbArchInit(void)
{
  return VbReadSharedMemory();
}
