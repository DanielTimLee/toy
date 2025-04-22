#!/bin/bash

source ./99_env.sh alpine "$@"
sudo rm -rf $ROOTFS_DIR $QEMU_IMG
[ "$(uname -m)" != "$TARGET" ] && docker run --rm --privileged multiarch/qemu-user-static --reset -p yes > /dev/null
[ "$TARGET" == "x86_64"  ] && APK_TOOLS_SHA256="1c65115a425d049590bec7c729c7fd88357fbb090a6fc8c31d834d7b0bc7d6f2"
[ "$TARGET" == "aarch64" ] && APK_TOOLS_SHA256="d49a63b8b6780fc1342d3e7e14862aa006c30bafbf74beec8e1dfe99e6f89471"
APK_TOOLS_URI=https://gitlab.alpinelinux.org/api/v4/projects/5/packages/generic/v2.14.0/$TARGET/apk.static
APK_OPTS="--allow-untrusted --no-progress"

wget https://raw.githubusercontent.com/alpinelinux/alpine-make-rootfs/v0.7.0/alpine-make-rootfs
chmod +x alpine-make-rootfs

mkdir .ssh && cp $HOME/.ssh/id_dsa* .ssh

export APK_OPTS APK_TOOLS_URI APK_TOOLS_SHA256
sudo -E ./alpine-make-rootfs --branch edge --packages \
        "alpine-base alpine-sdk linux-virt-dev linux-tools-dev util-linux shadow agetty \
        xterm bc bash openssl-dev openssh sshfs pahole ncurses-dev libtraceevent-dev devmem2" \
    --script-chroot $DIST_BASE.tar.gz - <<SHELL
        apk add --no-cache apk-tools

        echo 'console::respawn:/sbin/agetty --autologin root console' >> /etc/inittab
        echo 'nameserver 1.1.1.1' >> /etc/resolv.conf
        echo 'auto eth0' >> /etc/network/interfaces
        echo 'iface eth0 inet dhcp' >> /etc/network/interfaces

        rc-update add devfs sysinit
        rc-update add dmesg sysinit
        # rc-update add mdev sysinit
        rc-update add cgroups sysinit
        rc-update add hwdrivers sysinit

        rc-update add networking boot
        # rc-update add hwclock boot
        # rc-update add syslog boot
        rc-update add hostname boot
        rc-update add bootmisc boot
        rc-update add modules boot
        rc-update add seedrng boot
        rc-update add sysctl boot
        rc-update add swap boot
        rc-update add sshd boot

        rc-update add killprocs shutdown
        rc-update add savecache shutdown
        rc-update add mount-ro shutdown

        rc-update add local default
        rc-update -u

        # Login / sshfs / passwd / locale
        echo 'until sshfs $USER@10.0.2.2:/git /git -o reconnect; do sleep 1; done' >> /etc/bash/bashrc
        mkdir -p /root/.ssh && cp /mnt/.ssh/id_dsa /root/.ssh/id_ed25519
        cat /mnt/.ssh/id_dsa.pub >> /root/.ssh/authorized_keys
        echo 'PermitRootLogin yes' >> /etc/ssh/sshd_config
        chmod 600 /root/.ssh/authorized_keys
        echo 'resize' >> /etc/bash/bashrc
        echo 'root:root' | chpasswd
        chsh -s /bin/bash root
        mkdir /git
SHELL


# Generate disk image
dd if=/dev/zero of=$QEMU_IMG bs=1M count=10000
mkfs.ext4 $QEMU_IMG

# Copy root file system
sudo mkdir mnt
sudo mount -o loop $QEMU_IMG mnt
sudo tar -xzf $DIST_BASE.tar.gz -C mnt/

sudo umount mnt
sudo rmdir mnt

sudo rm -rf .ssh alpine-make-rootfs
