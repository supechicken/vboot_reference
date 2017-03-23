#!/bin/bash
#
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# 'Remount' rootfs using overlay.


NEW_ROOT_DIR="/home/new_root/"

ROOT_RW_DIR="${NEW_ROOT_DIR}root_rw/"
ROOT_RO_DIR="${NEW_ROOT_DIR}root_ro/"
UPPER_DIR="${ROOT_RW_DIR}upper/"
WORK_DIR="${ROOT_RW_DIR}work/"
OVERLAY_DIR="${ROOT_RW_DIR}overlay/"

NEW_ROOTFS_NAME="overlayroot"
PUT_OLD_FOLDER="put_old"

if [ "$(df -T | grep ${NEW_ROOTFS_NAME})" ];
then
  echo '[EXIT]','do nothing, since overlay rootfs is already mounted.'
else
  if [ ! -d "${ROOT_RW_DIR}" ]; then
    echo '[create new folder]:'${ROOT_RW_DIR}
    mkdir -p ${ROOT_RW_DIR}
  fi
  if [ ! -d "${ROOT_RO_DIR}" ]; then
    echo '[create new folder]:'${ROOT_RO_DIR}
    mkdir -p ${ROOT_RO_DIR}
  fi
  if [ ! -d "${UPPER_DIR}" ]; then
    echo '[create new folder]:'${UPPER_DIR}
    mkdir -p ${UPPER_DIR}
  fi
  if [ ! -d "${WORK_DIR}" ]; then
    echo '[create new folder]:'${WORK_DIR}
    mkdir -p ${WORK_DIR}
  fi
  if [ ! -d "${OVERLAY_DIR}" ]; then
    echo '[create new folder]:'${OVERLAY_DIR}
    mkdir -p ${OVERLAY_DIR}
  fi

  if [ -n "$(ls -A ${ROOT_RO_DIR})" ];
  then
    echo '[FAIL]',${ROOT_RO_DIR},' is not clean for operation.'
  else
    mount -o bind / ${ROOT_RO_DIR}
    mount -t overlay ${NEW_ROOTFS_NAME} \
      -olowerdir=${ROOT_RO_DIR},upperdir=${UPPER_DIR},workdir=${WORK_DIR} \
      ${OVERLAY_DIR}
    mkdir -p ${OVERLAY_DIR}${PUT_OLD_FOLDER}/
    ### cd ${OVERLAY_DIR}
    ### pivot_root . /home/new_root/root_rw/overlay/put_old/
    pivot_root ${OVERLAY_DIR} ${OVERLAY_DIR}${PUT_OLD_FOLDER}/
    mount --move /${PUT_OLD_FOLDER}/dev/ /dev
    mount --move /${PUT_OLD_FOLDER}/proc/ /proc
    echo '[SUCCESS]',${ROOT_RO_DIR},' new rootfs is mounted amazingly! '
  fi
fi

#mount --move /put_old/mnt/stateful_partition/ /mnt/stateful_partition/
#chroot /

