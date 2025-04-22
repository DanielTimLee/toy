#!/bin/bash

DIST=${DIST:-${1:-ubuntu}}
TARGET=${TARGET:-${2:-x86_64}}
KERN_VER=${KERN_VER:-${3:-v6.14}}
KERN_BASE=$KERN_VER-$TARGET

DIST_BASE=$DIST-$TARGET
QEMU_IMG=$DIST_BASE.img
ROOTFS_DIR="/tmp/$DIST_BASE-rootfs"
MAKEFLAGS=-j$(nproc)

case "$TARGET" in
  x86_64)
    ARCH=x86_64
    DEB_ARCH=amd64
    APT_EXTRA="qemu-system-x86"
    KERN_OPT="console=ttyS0 ibt=off"
    KERN_IMG=arch/$ARCH/boot/bzImage
    QEMU_EXTRA="-M pc -cpu qemu64,pmu=on"
    ;;
  aarch64)
    if [[ -f qemu.dtb ]]; then
        DTB="-dtb qemu.dtb"
    fi
    ARCH=arm64
    DEB_ARCH=arm64
    KERN_OPT="console=ttyAMA0"
    KERN_IMG=arch/$ARCH/boot/Image
    APT_EXTRA="qemu-system-aarch64"
    QEMU_EXTRA="-M virt -cpu cortex-a72 $DTB"
    ;;
  *)
    echo "Unsupported TARGET: $TARGET" >&2
    exit 1
    ;;
esac


if [[ "$(uname -m)" != "$TARGET" ]]; then
  case "$TARGET" in
    x86_64)
      CROSS_COMPILE=x86_64-linux-gnu-
      APT_EXTRA="$APT_EXTRA gcc-x86-64-linux-gnu"
      ;;
    aarch64)
      CROSS_COMPILE=aarch64-linux-gnu-
      APT_EXTRA="$APT_EXTRA gcc-aarch64-linux-gnu device-tree-compiler"
      ;;
  esac
else
    QEMU_EXTRA="${QEMU_EXTRA} -accel kvm"
fi

# SSH Key settings (id_dsa is deprecated)
if [[ ! -f $HOME/.ssh/id_dsa.pub ]]; then
    cd $HOME/.ssh
    ssh-keygen -t ed25519 -f id_dsa -N ""
    cat id_dsa.pub >> authorized_keys
    chmod 600 authorized_keys
    cd -
fi

export DIST TARGET KERN_VER KERN_BASE DIST_BASE KERN_IMG ROOTFS_DIR \
    QEMU_IMG QEMU_EXTRA MAKEFLAGS DEB_ARCH ARCH CROSS_COMPILE APT_EXTRA
