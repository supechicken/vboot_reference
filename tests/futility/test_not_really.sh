#!/bin/bash -eu

me=${0##*/}

TMP="$OUTDIR/$me.tmp"

echo "FUTILITY=$FUTILITY" > "$TMP"
echo "SCRIPTDIR=$SCRIPTDIR" >> "$TMP"

exit 0
