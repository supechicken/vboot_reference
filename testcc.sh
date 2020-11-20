#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
# adapted from coreboot's util/xcompile/xcompile
TMPFILE="$(mktemp /tmp/temp.XXXXXX 2>/dev/null || echo /tmp/temp.coreboot.$RANDOM)"
testcc() {
	cc="$1"
	cflags="$2"
	tmp_c="$TMPFILE.c"
	tmp_o="$TMPFILE.o"
	rm -f "$tmp_c" "$tmp_o"
	echo "void _start(void) {}" >"$tmp_c"
	"$cc" -nostdlib -Werror $cflags -c "$tmp_c" -o "$tmp_o" >/dev/null 2>&1
}

flag_supported() {
	testcc "$CC" "$1" && echo "$1"
}

flag_supported -Wimplicit-fallthrough
flag_supported -Wno-address-of-packed-member
flag_supported -Wno-unknown-warning

if [ -n "$TMPFILE" ]; then
	rm -f "$TMPFILE" "$TMPFILE.c" "$TMPFILE.o"
fi
