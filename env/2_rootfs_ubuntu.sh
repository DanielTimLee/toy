#!/bin/bash

source ./99_env.sh ubuntu "$@"
sudo rm -rf $ROOTFS_DIR $QEMU_IMG
# DEBOOTSTRAP=debootstrap
DEBOOTSTRAP="mmdebstrap --mode=unshare"

# perf build
# libzstd1 libdwarf-dev libdw-dev binutils-dev libcap-dev libelf-dev libnuma-dev python3 python3-dev libssl-dev libunwind-dev libdwarf-dev zlib1g-dev liblzma-dev libaio-dev libtraceevent-dev debuginfod libpfm4-dev libslang2-dev systemtap-sdt-dev libperl-dev binutils-dev libbabeltrace-dev libiberty-dev libzstd-dev
packages=(
    ubuntu-server ubuntu-standard openssh-server build-essential
    xterm bc bison flex sshfs pahole dhcpcd5 clang lld
    memtool libncurses-dev libelf-dev libssl-dev pkg-config elfutils
)
packages=$(IFS=, && echo "${packages[*]}")

# Ubuntu rootfs (noble, 24.04)
sudo $DEBOOTSTRAP --components=main,universe,restricted --arch=$DEB_ARCH \
    --include="$packages" noble $ROOTFS_DIR http://192.168.214.3/repository/ubuntu

# ssh key
sudo mkdir -p $ROOTFS_DIR/root/.ssh
sudo cp $HOME/.ssh/id_dsa* $ROOTFS_DIR/root/.ssh

# Login / sshfs / passwd / locale
sudo chroot $ROOTFS_DIR sh -c "
    cp /etc/skel/.* /root/
    echo 'root:root' | chpasswd
    echo 'en_US.UTF-8 UTF-8' >> /etc/locale.gen
    echo 'ko_KR.UTF-8 UTF-8' >> /etc/locale.gen
    echo 'PermitRootLogin yes' >> /etc/ssh/sshd_config

    sed -i 's|^kernel|#&' /etc/sysctl.d/10-console-messages.conf
    echo 'until sshfs $USER@10.0.2.2:/git /git -o reconnect; do sleep 1; done' >> /root/.bashrc
    sed -i 's|/sbin/agetty.*|/sbin/agetty --autologin root - \$TERM|' /lib/systemd/system/serial-getty@.service

    cat /root/.ssh/id_dsa.pub >> /root/.ssh/authorized_keys
    chmod 600 /root/.ssh/authorized_keys
    echo 'resize' >> /root/.bashrc
    mkdir -p /root/.ssh
    mkdir /git
    locale-gen
"

# backup ROOTFS
sudo tar -czf $DIST_BASE.tar.gz $ROOTFS_DIR

# Generate disk image
dd if=/dev/zero of=$QEMU_IMG bs=1M count=10000
mkfs.ext4 $QEMU_IMG

# Copy root file system
sudo mkdir mnt
sudo mount -o loop $QEMU_IMG mnt
sudo rsync -a $ROOTFS_DIR/ mnt/

sudo mkdir -p mnt/git
sudo umount mnt
sudo rmdir mnt

sudo rm -rf $ROOTFS_DIR
