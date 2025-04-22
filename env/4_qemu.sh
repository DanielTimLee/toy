#/bin/bash

source ./99_env.sh "$@"
QEMU_EXTRA="$QEMU_EXTRA -smp 8"
QEMU_MEM="512M"

qemu-system-$TARGET $QEMU_EXTRA -m "$QEMU_MEM" \
    -kernel "/git/src/$KERN_BASE/$KERN_IMG" \
    -append "root=/dev/vda rw $KERN_OPT" \
    -drive file="$QEMU_IMG",format=raw,if=virtio \
    -netdev user,id=mynet0,hostfwd=tcp::22222-:22 \
    -device virtio-net-pci,netdev=mynet0 \
    -nographic

# To enable uio_pdrv_genirq append below
# uio_pdrv_genirq.of_id=generic-uio
#
# To mount userspace /git
# sshfs $USER@10.0.2.2:/git /git
#
# To start KGDB debugging session
# kgdbwait kgdboc=ttyS0,115200
#
# If you want IOMMU to be used,
# -M q35
#
# To set kernel log level, append:
# loglevel=4 (KERN_WARNING)
#
# To prevent TSC (Time Stamp Counter) skew
# 1. Replace to PARAVIRT with KVM_GUEST
# 2. Or use -rtc clock=vm (eventually skew)
#
# To start qemu with EDU device
# -device edu
#
# Or with qemu monitor
# (qemu) device_add edu
# (qemu) device_del edu
# echo 1 > /sys/bus/pci/rescan
