/* Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Host functions for verified boot.
 */

/* TODO: change all 'return 0', 'return 1' into meaningful return codes */

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cryptolib.h"
#include "host_common.h"
#include "vboot_common.h"

void Fatal(const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  fprintf(stderr, "ERROR: ");
  vfprintf(stderr, format, ap);
  va_end(ap);
  exit(1);
}

void Warning(const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  fprintf(stderr, "WARNING: ");
  vfprintf(stderr, format, ap);
  va_end(ap);
}

char* StrCopy(char* dest, const char* src, int dest_size) {
  strncpy(dest, src, dest_size);
  dest[dest_size - 1] = '\0';
  return dest;
}


uint8_t* ReadFile(const char* filename, uint64_t* sizeptr) {
  FILE* f;
  uint8_t* buf;
  uint64_t size;

  f = fopen(filename, "rb");
  if (!f) {
    VBDEBUG(("Unable to open file %s\n", filename));
    return NULL;
  }

  fseek(f, 0, SEEK_END);
  size = ftell(f);
  rewind(f);

  buf = malloc(size);
  if (!buf) {
    fclose(f);
    return NULL;
  }

  if(1 != fread(buf, size, 1, f)) {
    VBDEBUG(("Unable to read from file %s\n", filename));
    fclose(f);
    free(buf);
    return NULL;
  }

  fclose(f);
  if (sizeptr)
    *sizeptr = size;
  return buf;
}


char* ReadFileString(char* dest, int size, const char* filename) {
  char* got;
  FILE* f;

  f = fopen(filename, "rt");
  if (!f)
    return NULL;

  got = fgets(dest, size, f);
  fclose(f);
  return got;
}


int ReadFileInt(const char* filename) {
  char buf[64];
  int value;
  char* e = NULL;

  if (!ReadFileString(buf, sizeof(buf), filename))
    return -1;

  /* Convert to integer.  Allow characters after the int ("123 blah"). */
  value = strtol(buf, &e, 0);
  if (e == buf)
    return -1;  /* No characters consumed, so conversion failed */

  return value;
}


int ReadFileBit(const char* filename, int bitmask) {
  int value = ReadFileInt(filename);
  if (value == -1)
    return -1;
  else return (value & bitmask ? 1 : 0);
}


int WriteFile(const char* filename, const void *data, uint64_t size) {
  FILE *f = fopen(filename, "wb");
  if (!f) {
    VBDEBUG(("Unable to open file %s\n", filename));
    return 1;
  }

  if (1 != fwrite(data, size, 1, f)) {
    VBDEBUG(("Unable to write to file %s\n", filename));
    fclose(f);
    unlink(filename);  /* Delete any partial file */
  }

  fclose(f);
  return 0;
}

void PrintPubKeySha1Sum(VbPublicKey* key) {
  uint8_t* buf = ((uint8_t *)key) + key->key_offset;
  uint64_t buflen = key->key_size;
  uint8_t* digest = DigestBuf(buf, buflen, SHA1_DIGEST_ALGORITHM);
  int i;
  for (i=0; i<SHA1_DIGEST_SIZE; i++)
    printf("%02x", digest[i]);
  free(digest);
}

/****************************************************************************/
/* Modify original argc/argv to preload options from a config file.
 *
 * The name of the config file is based on argv[0]. The directories to search
 * for the file are hard-coded, but that list can be replaced or prepended by
 * an environment variable, also based on argv[0]. For example:
 *
 * argv[0] is /some/path/to/foo
 *
 * The program name is "foo".
 *
 * The config file is named "foo.options"
 *
 * The environment variable is $FOO_OPTIONS_PATH
 *
 * The default list of directories to search for foo.options is determined at
 * compile time, but the environment variable can modify that list. If it
 * exists, the variable should contain a colon-separated list of directories.
 * If the variable ends with a colon, the variable list is prepended to the
 * default list; otherwise it replaces it.
 *
 * The first config file that is found is the only one that is used. The
 * contents of the config file are prepended to the command-line argc/argv.
 * The config file is text, with one parameter per line. Every line and all
 * whitespace is significant, which allows parameters including spaces and
 * empty strings to be specified. So, if foo.options contains these three
 * lines:
 *
 *   -p
 *   2
 *   -x 3
 *
 * Then the result is that the original argc/argv will be modified so that
 * argv[1], argv[2], argv[3] are now
 *
 *   "-p"  "2"  "-x 3"
 *
 * and the rest of the original argv array is shoved over to make room for
 * these new args. Note that there is a (fairly large) limit on the length of
 * any individual line of the config file.
 */
#define DEFAULT_DIRLIST "/usr/share/vboot/config:/etc/vboot/config"
#define CFGFILE_APPEND ".options"
#define VARNAME_APPEND "_OPTIONS_PATH"
void PreloadOptions(int *argcp, char ***argvp)
{
  int i, len;
  int argc = *argcp;
  char **argv = *argvp;
  int new_argc;
  char **new_argv;
  char *progname;
  char *cfgname;
  char *varname;
  char *dirlist;
  char *s, *dir;
  char filename[PATH_MAX];
  char linebuf[PATH_MAX];
  int linecount;
  FILE *fp = 0;

  progname = strrchr(argv[0], '/');
  if (progname)
    progname++;
  else
    progname = argv[0];

  len = strlen(progname) + strlen(CFGFILE_APPEND) + 1;
  cfgname = malloc(len);
  sprintf(cfgname, "%s" CFGFILE_APPEND, progname);

  len = strlen(progname) + strlen(VARNAME_APPEND) + 1;
  varname = malloc(len);
  sprintf(varname, "%s" VARNAME_APPEND, progname);
  for (i=0; varname[i]; i++) {
    varname[i] = toupper(varname[i]);
    if (varname[i] == '.')
      varname[i] = '_';
  }

  s = getenv(varname);
  len = s ? strlen(s) : 0;
  if (len) {
    if (s[len-1] == ':') {
      len += strlen(DEFAULT_DIRLIST);
      dirlist = malloc(len+1);
      sprintf(dirlist, "%s" DEFAULT_DIRLIST, s);
    } else {
      dirlist = strdup(s);
    }
  } else {
    dirlist = strdup(DEFAULT_DIRLIST);
  }

  for (s=dirlist; (dir=strtok(s, ":")); s=0) {
    sprintf(filename, "%s/%s", dir, cfgname);

    fp = fopen(filename, "r");
    if (fp)
      break;
  }

  if (fp) {
    // We're gonna add some args. To figure out how many, I'll just scan the
    // file twice. That's probably slowest, but it's also the simplest.
    for (linecount=0; (fgets(linebuf, PATH_MAX, fp)); linecount++)
      /* nothing */;

    new_argc = argc + linecount;
    new_argv = (char **)malloc((new_argc + 1) * sizeof(char *));
    new_argv[0] = argv[0];             // just copy original pointer

    rewind(fp);
    for (i=1; (fgets(linebuf, PATH_MAX, fp)); i++) {
      s = strrchr(linebuf, '\n');
      if (s)
        *s = '\0';
      s = strrchr(linebuf, '\r');       // fine, chomp DOS too
      if (s)
        *s = '\0';
      new_argv[i] = strdup(linebuf);
    }
    fclose(fp);

    for (; i < new_argc; i++)
      new_argv[i] = argv[i - linecount]; // copy the rest of the pointers

    new_argv[new_argc] = 0;

    *argcp = new_argc;
    *argvp = new_argv;

    fprintf(stderr, "\nWARNING: %s is preloading arguments from %s\n\n",
            argv[0], filename);
  }

  free(dirlist);
  free(varname);
  free(cfgname);
}
