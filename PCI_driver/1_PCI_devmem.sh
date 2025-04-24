#!/bin/bash
ID=1234:11e8                 # EDU PCI device ID
BASE=$(setpci -d $ID 0x10.l)

echo "===================================================="
echo "PCI BAR0 raw address is: 0x$BASE"
echo "===================================================="

./99_MMIO_test.sh $BASE
