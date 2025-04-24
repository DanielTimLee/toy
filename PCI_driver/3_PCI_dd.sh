#!/bin/bash
ID=1234:11e8                 # EDU PCI device ID

insmod 3_PCI.ko
_type="edu_pci"
DEV=/dev/$_type
major="$(grep $_type /proc/devices | cut -d' ' -f1)"
mknod $DEV c "$major" 0

DD="dd status=none count=1 bs=4"
WRITE="$DD of=$DEV"
READ="$DD if=$DEV"
OD="od -An -t x4"

echo "===================================================="
echo "${ID} is mapped to $DEV"
echo "===================================================="

./99_MMIO_test.sh     0 $DEV

echo "===================================================="
echo "Manual R/W by offset with $DEV"
echo "===================================================="

echo "# 0x00 (RO) : identification (0x010000edu)"
$READ skip=0 | $OD                              # 00000000: 010000ed (Magic number)

echo "# 0x04 (RW) : value inversion (~0x00000000 ~0x000000ff)"
printf '\x00\x00\x00\x00' | $WRITE seek=1
$READ skip=1 | $OD                              # 00000004: ffffffff (Negate of 0x00000000)
printf '\xFF\x00\x00\x00' | $WRITE seek=1
$READ skip=1 | $OD                              # 00000004: ffffff00 (Negate of 0x000000FF)

echo "# 0x08 (RW) : factorial computation (4! = 24 / 8! = 40320)"
printf '\x04\x00\x00\x00' | $WRITE seek=2
$READ skip=2 | $OD                              # 00000008: 00000018 (4! = 24 0x18)
printf '\x08\x00\x00\x00' | $WRITE seek=2
$READ skip=2 | $OD                              # 00000008: 00009D80 (8! = 40320 0x9D80)

echo "# 0x20 (RW) : status register (0x01 = WIP, 0x80 = raise interrupt)"
echo "# 0x24 (RO) : interrupt status register (0x01 = interrupt raised)"
printf '\x80\x00\x00\x00' | $WRITE seek=8       # 8*4 = 32 (0x20)
printf '\x80\x96\x98\x00' | $WRITE seek=2       # 10,000,000
$READ skip=8 | $OD                              # 00000020: 00000081 (0x01 + 0x80: still running)
sleep 1
$READ skip=8 | $OD                              # 00000020: 00000080 (0x80)
$READ skip=2 | $OD                              # 00000008: 00000000 (overflowed)
dmesg | tail -1

echo "# 0x60 (WO) : interrupt raise register (set)"
echo "# 0x64 (WO) : interrupt acknowledge register (clear)"
printf '\x01\x00\x00\x00' | $WRITE seek=24      # 24*4 = 96  (0x60) manual interrupt on
dmesg | tail -1

cat /proc/interrupts | grep $_type

rm $DEV
rmmod 3_PCI
