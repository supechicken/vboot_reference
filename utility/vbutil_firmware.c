/* Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Verified boot firmware utility
 */

#include <getopt.h>
#include <inttypes.h>  /* For PRIu64 */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cryptolib.h"
#include "host_common.h"
#include "kernel_blob.h"
#include "vboot_common.h"

/* global options */
static int opt_preamble_fmt = 2;        /* 0 = create v3, verify either */

/* Command line options */
enum {
  OPT_MODE_VBLOCK = 1000,
  OPT_MODE_VERIFY,
  OPT_KEYBLOCK,
  OPT_SIGNPUBKEY,
  OPT_SIGNPRIVATE,
  OPT_VERSION,
  OPT_FV,
  OPT_KERNELKEY,
  OPT_FLAGS,
  OPT_NAME,
  OPT_FORMAT,
};

static struct option long_opts[] = {
  {"vblock", 1, 0,                    OPT_MODE_VBLOCK             },
  {"verify", 1, 0,                    OPT_MODE_VERIFY             },
  {"keyblock", 1, 0,                  OPT_KEYBLOCK                },
  {"signpubkey", 1, 0,                OPT_SIGNPUBKEY              },
  {"signprivate", 1, 0,               OPT_SIGNPRIVATE             },
  {"version", 1, 0,                   OPT_VERSION                 },
  {"fv", 1, 0,                        OPT_FV                      },
  {"kernelkey", 1, 0,                 OPT_KERNELKEY               },
  {"flags", 1, 0,                     OPT_FLAGS                   },
  {"name", 1, 0,                      OPT_NAME                    },
  {"format", 1, 0,                    OPT_FORMAT                  },
  {NULL, 0, 0, 0}
};


/* Print help and return error */
static int PrintHelp(void) {

  puts("vbutil_firmware - Verified boot key block utility\n"
       "\n"
       "Usage:  vbutil_firmware <--vblock|--verify> <file> [OPTIONS]\n"
       "\n"
       "For '--vblock <file>', required OPTIONS are:\n"
       "  --keyblock <file>           Key block in .keyblock format\n"
       "  --signprivate <file>        Signing private key in .vbprivk format\n"
       "  --version <number>          Firmware version\n"
       "  --fv <file>                 Firmware volume to sign\n"
       "  --kernelkey <file>          Kernel subkey in .vbpubk format\n"
       "optional OPTIONS are:\n"
       "  --flags <number>            Preamble flags (defaults to 0)\n"
       "  --name <string>             Human-readable description\n"
       "  --format <number>           Use 3 for new platforms, 2 for existing\n"
       "\n"
       "For '--verify <file>', required OPTIONS are:\n"
       "  --signpubkey <file>         Signing public key in .vbpubk format\n"
       "  --fv <file>                 Firmware volume to verify\n"
       "\n"
       "For '--verify <file>', optional OPTIONS are:\n"
       "  --kernelkey <file>          Write the kernel subkey to this file\n"
       "");
  return 1;
}



/* Returns the flags from a firmware preamble, or a default value for
 * older preamble versions which didn't contain flags.  Use this
 * function to ensure compatibility with older preamble versions
 * (2.0).  Assumes the preamble has already been verified via
 * VerifyFirmwarePreamble(). */
uint32_t VbGetFirmwarePreambleFlags(const VbFirmwarePreambleUnion* preamble) {
  if (IS_V3(preamble))
      return preamble->v3.flags;

  /* Old 2.0 structure; return default flags. */
  if (preamble->m.header_version_minor < 1)
    return 0;

  return preamble->v2.flags;
}


/* Create a firmware .vblock */
static int Vblock(const char* outfile, const char* keyblock_file,
                  const char* signprivate, uint64_t version,
                  const char* fv_file, const char* kernelkey_file,
                  uint32_t preamble_flags, const char *name) {

  VbPrivateKey* signing_key;
  VbPublicKey* kernel_subkey;
  VbSignature* body_digest;
  VbFirmwarePreambleUnion* preamble;
  VbKeyBlockHeader* key_block;
  uint64_t key_block_size;
  uint8_t* fv_data;
  uint64_t fv_size;
  FILE* f;
  uint64_t i;

  if (!outfile) {
    Fatal("Must specify output filename\n");
  }
  if (!keyblock_file || !signprivate || !kernelkey_file) {
    Fatal("Must specify all keys\n");
  }
  if (!fv_file) {
    Fatal("Must specify firmware volume\n");
  }

  if (name && strlen(name)+1 > sizeof(preamble->v3.name)) {
    Fatal("Name string is too long\n");
  }

  /* Read the key block and keys */
  key_block = (VbKeyBlockHeader*)ReadFile(keyblock_file, &key_block_size);
  if (!key_block) {
    Fatal("Error reading key block.\n");
  }

  signing_key = PrivateKeyRead(signprivate);
  if (!signing_key) {
    Fatal("Error reading signing key.\n");
  }

  kernel_subkey = PublicKeyRead(kernelkey_file);
  if (!kernel_subkey) {
    Fatal("Error reading kernel subkey.\n");
  }

  /* Read and sign the firmware volume */
  fv_data = ReadFile(fv_file, &fv_size);
  if (!fv_data)
    return 1;
  if (!fv_size) {
    Fatal("Empty firmware volume file\n");
  }

  /* Create preamble */
  if (2 == opt_preamble_fmt) {
    body_digest = CalculateSignature(fv_data, fv_size, signing_key);
    if (!body_digest) {
      Fatal("Error calculating body signature\n");
    }
    preamble = (VbFirmwarePreambleUnion*)CreateFirmwarePreamble2_1(
      version, kernel_subkey, body_digest, signing_key, preamble_flags);
  } else {
    body_digest = CalculateHash(fv_data, fv_size, signing_key);
    if (!body_digest) {
      Fatal("Error calculating body digest\n");
    }
    preamble = (VbFirmwarePreambleUnion*)CreateFirmwarePreamble(
      version, kernel_subkey, body_digest, signing_key, preamble_flags, name);
  }
  if (!preamble) {
    Fatal("Error creating preamble.\n");
  }
  free(fv_data);

  /* Write the output file */
  f = fopen(outfile, "wb");
  if (!f) {
    Fatal("Can't open output file %s\n", outfile);
  }
  i = ((1 != fwrite(key_block, key_block_size, 1, f)) ||
       (1 != fwrite(preamble, preamble->m.preamble_size, 1, f)));
  fclose(f);
  if (i) {
    Fatal("Can't write output file %s\n", outfile);
    unlink(outfile);
  }

  /* Success */
  return 0;
}

static int Verify(const char* infile, const char* signpubkey,
                  const char* fv_file, const char* kernelkey_file) {

  VbKeyBlockHeader* key_block;
  VbFirmwarePreambleUnion* preamble;
  VbPublicKey* data_key;
  VbPublicKey* sign_key;
  VbPublicKey* kernel_subkey;
  RSAPublicKey* rsa;
  uint8_t* blob;
  uint64_t blob_size;
  uint8_t* fv_data;
  uint64_t fv_size;
  uint64_t now = 0;
  uint32_t flags;

  if (!infile || !signpubkey || !fv_file) {
    Fatal("Must specify filename, signpubkey, and fv\n");
  }

  /* Read public signing key */
  sign_key = PublicKeyRead(signpubkey);
  if (!sign_key) {
    Fatal("Error reading signpubkey.\n");
  }

  /* Read blob */
  blob = ReadFile(infile, &blob_size);
  if (!blob) {
    Fatal("Error reading input file\n");
  }

  /* Read firmware volume */
  fv_data = ReadFile(fv_file, &fv_size);
  if (!fv_data) {
    Fatal("Error reading firmware volume\n");
  }

  /* Verify key block */
  key_block = (VbKeyBlockHeader*)blob;
  if (0 != KeyBlockVerify(key_block, blob_size, sign_key, 0)) {
    Fatal("Error verifying key block.\n");
  }
  free(sign_key);
  now += key_block->key_block_size;

  printf("Key block:\n");
  data_key = &key_block->data_key;
  printf("  Size:                %" PRIu64 "\n", key_block->key_block_size);
  printf("  Flags:               %" PRIu64 " (ignored)\n",
         key_block->key_block_flags);
  printf("  Data key algorithm:  %" PRIu64 " %s\n", data_key->algorithm,
         (data_key->algorithm < kNumAlgorithms ?
          algo_strings[data_key->algorithm] : "(invalid)"));
  printf("  Data key version:    %" PRIu64 "\n", data_key->key_version);
  printf("  Data key sha1sum:    ");
  PrintPubKeySha1Sum(data_key);
  printf("\n");

  rsa = PublicKeyToRSA(&key_block->data_key);
  if (!rsa) {
    Fatal("Error parsing data key.\n");
  }

  /* Verify preamble */
  preamble = (VbFirmwarePreambleUnion*)(blob + now);
  if (IS_V3(preamble)) {
    if (2 == opt_preamble_fmt) {
      Fatal("Preamble is v3, accepting v2 only.\n");
    }
    if (0 != VerifyFirmwarePreamble(&preamble->v3, blob_size - now, rsa)) {
      Fatal("Error verifying v3 preamble.\n");
    }
  } else {
    if (3 == opt_preamble_fmt) {
      Fatal("Preamble is v2, accepting v3 only.\n");
    }
    if (0 != VerifyFirmwarePreamble2_x(&preamble->v2, blob_size - now, rsa)) {
      Fatal("Error verifying v2 preamble.\n");
    }
  }

  now += preamble->m.preamble_size;

  printf("Preamble:\n");
  printf("  Size:                  %" PRIu64 "\n", preamble->m.preamble_size);
  printf("  Header version:        %" PRIu32 ".%" PRIu32"\n",
         preamble->m.header_version_major, preamble->m.header_version_minor);
  printf("  Firmware version:      %" PRIu64 "\n",
         FP_MEMBER(preamble, firmware_version));
  if (IS_V3(preamble))
    kernel_subkey = &preamble->v3.kernel_subkey;
  else
    kernel_subkey = &preamble->v2.kernel_subkey;
  printf("  Kernel key algorithm:  %" PRIu64 " %s\n",
         kernel_subkey->algorithm,
         (kernel_subkey->algorithm < kNumAlgorithms ?
          algo_strings[kernel_subkey->algorithm] : "(invalid)"));
  printf("  Kernel key version:    %" PRIu64 "\n",
         kernel_subkey->key_version);
  printf("  Kernel key sha1sum:    ");
  PrintPubKeySha1Sum(kernel_subkey);
  printf("\n");
  printf("  Firmware body size:    %" PRIu64 "\n",
         FP_MEMBER_V3_V2(preamble, body_digest, body_signature).data_size);
  flags = VbGetFirmwarePreambleFlags(preamble);
  printf("  Preamble flags:        %" PRIu32 "\n", flags);
  if (IS_V3(preamble))
    printf("  Name:                  %s\n", preamble->v3.name);

  /* TODO: verify body size same as signature size */

  /* Verify body */
  if (flags & VB_FIRMWARE_PREAMBLE_USE_RO_NORMAL) {
    printf("Preamble requests USE_RO_NORMAL; skipping body verification.\n");
  } else {
    if (IS_V3(preamble)) {
      if (0 != EqualData(fv_data, fv_size, &preamble->v3.body_digest, rsa)) {
        Fatal("Error verifying (v3) firmware body.\n");
      }
    } else {
      if (0 != VerifyData(fv_data, fv_size, &preamble->v2.body_signature,
                          rsa)) {
        Fatal("Error verifying (v2) firmware body.\n");
      }
    }
    printf("Body verification succeeded.\n");
  }

  if (kernelkey_file) {
    if (0 != PublicKeyWrite(kernelkey_file, kernel_subkey)) {
      fprintf(stderr,
              "vbutil_firmware: unable to write kernel subkey\n");
    }
  }

  return 0;
}


int main(int argc, char* argv[]) {

  char* filename = NULL;
  char* key_block_file = NULL;
  char* signpubkey = NULL;
  char* signprivate = NULL;
  uint64_t version = 0;
  char* fv_file = NULL;
  char* kernelkey_file = NULL;
  char* name = NULL;
  uint32_t preamble_flags = 0;
  int mode = 0;
  int parse_error = 0;
  char* e;
  int i;

  while ((i = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
    switch (i) {
      case '?':
        /* Unhandled option */
        printf("Unknown option\n");
        parse_error = 1;
        break;

      case OPT_MODE_VBLOCK:
      case OPT_MODE_VERIFY:
        mode = i;
        filename = optarg;
        break;

      case OPT_KEYBLOCK:
        key_block_file = optarg;
        break;

      case OPT_SIGNPUBKEY:
        signpubkey = optarg;
        break;

      case OPT_SIGNPRIVATE:
        signprivate = optarg;
        break;

      case OPT_FV:
        fv_file = optarg;
        break;

      case OPT_KERNELKEY:
        kernelkey_file = optarg;
        break;

      case OPT_VERSION:
        version = strtoul(optarg, &e, 0);
        if (!*optarg || (e && *e)) {
          printf("Invalid --version\n");
          parse_error = 1;
        }
        break;

      case OPT_FLAGS:
        preamble_flags = strtoul(optarg, &e, 0);
        if (!*optarg || (e && *e)) {
          printf("Invalid --flags\n");
          parse_error = 1;
        }
        break;

      case OPT_NAME:
        name = optarg;
        break;

      case OPT_FORMAT:
        opt_preamble_fmt = strtoul(optarg, &e, 0);
        if (!*optarg || (e && *e) ||
            (opt_preamble_fmt != 2 && opt_preamble_fmt != 3)) {
          printf("Invalid --format\n");
          parse_error = 1;
        }
        break;
    }
  }

  if (parse_error)
    return PrintHelp();

  switch(mode) {
    case OPT_MODE_VBLOCK:
      return Vblock(filename, key_block_file, signprivate, version, fv_file,
                    kernelkey_file, preamble_flags, name);
    case OPT_MODE_VERIFY:
      return Verify(filename, signpubkey, fv_file, kernelkey_file);
    default:
      printf("Must specify a mode.\n");
      return PrintHelp();
  }
}
