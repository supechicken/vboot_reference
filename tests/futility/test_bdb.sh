#!/bin/bash -eux
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

me=${0##*/}
TMP="$me.tmp"

# Work in scratch directory
cd "$OUTDIR"
BDB_FILE=bdb.bin

TESTKEY_DIR=${SRCDIR}/tests/testkeys
DATA_DIR=${SRCDIR}/tests/testdata

BDBKEY_PUB=${TESTKEY_DIR}/bdbkey.keyb
BDBKEY_PRI=${TESTKEY_DIR}/bdbkey.pem
DATAKEY_PUB=${TESTKEY_DIR}/datakey.keyb
DATAKEY_PRI=${TESTKEY_DIR}/datakey.pem
KEYDIGEST_FILE=${DATA_DIR}/bdbkey_digest.bin

verify() {
	${FUTILITY} bdb --verify ${BDB_FILE} --key_digest ${KEYDIGEST_FILE}
}

# Demonstrate bdb --create can create a valid BDB
${FUTILITY} bdb --create ${BDB_FILE} \
	--bdbkey_pri ${BDBKEY_PRI} --bdbkey_pub ${BDBKEY_PUB} \
	--datakey_pub ${DATAKEY_PUB} --datakey_pri ${DATAKEY_PRI}
verify

# cleanup
rm -rf ${TMP}*
exit 0
