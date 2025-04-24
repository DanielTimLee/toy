#!/bin/bash
ID=1234:11e8                 # EDU PCI device ID
# BASE=$(setpci -d $ID 0x10.l)

modprobe uio_pci_generic
echo '1234 11e8' > /sys/bus/pci/drivers/uio_pci_generic/new_id

cd /dev
UIO=$(ls uio*)
UIO_MAP="/sys/class/uio/$UIO/maps/map0/addr"
cd -
echo "===================================================="
echo "${ID} is mapped to /dev/$UIO"
echo "PCI BAR0 is mmaped by UIO: $UIO_MAP"
echo "===================================================="

./99_MMIO_test.sh     0 /dev/$UIO

UIO_MAP_ADDR=$(cat $UIO_MAP)           # 0x00000000fea00000
BASE=${UIO_MAP_ADDR:8}                 # fea00000 (stripped)

echo "===================================================="
echo "PCI BAR0 raw address is: 0x$BASE"
echo "===================================================="

./99_MMIO_test.sh $BASE
