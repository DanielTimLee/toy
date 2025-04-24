#!/bin/bash
ID=1234:11e8                 # EDU PCI device ID

insmod 4_PCI_dma.ko # use_coherent=0 for manual page mapping
_type="edu_pci"
DEV=/dev/$_type
major="$(grep $_type /proc/devices | cut -d' ' -f1)"
mknod $DEV c "$major" 0

DD="dd status=none count=1 bs=4096"
TEMP_FILE="temp_data.bin"
TEST_FILE="test_data.bin"

echo "===================================================="
echo "DMA R/W with $DEV"
echo "===================================================="

echo "Generating random to temp file"
$DD if=/dev/urandom of=$TEMP_FILE
ls -al $TEMP_FILE

echo "Writing temp file to the device with DMA"
$DD if=$TEMP_FILE   of=$DEV

echo "Reading device to test file with DMA"
$DD if=$DEV         of=$TEST_FILE

echo "Comapring $TEMP_FILE and $TEST_FILE"
diff $TEMP_FILE $TEST_FILE > /dev/null
if [ $? -eq 0 ]; then
    echo "Test Complete"
else
    echo "Test Failed"
fi

rm $DEV
rmmod 4_PCI_dma
