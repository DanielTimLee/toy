#!/bin/bash

source ./99_env.sh "$@"
MOUNT_DIR="$PWD/mnt"

if [[ ! -f "$QEMU_IMG" ]]; then
    echo "$QEMU_IMG not found."
    exit 1
fi

sudo mkdir -p $MOUNT_DIR
sudo mount -o loop $QEMU_IMG $MOUNT_DIR

cd /git/src/$KERN_BASE
sudo make modules_install INSTALL_MOD_PATH=$MOUNT_DIR
cd -

sudo umount $MOUNT_DIR
sudo rmdir $MOUNT_DIR
