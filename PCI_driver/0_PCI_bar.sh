#!/bin/bash
ID=1234:11e8                 # EDU PCI device ID

# How PCI figures out BAR size?
#
# (qemu) device_add edu
# dmesg -w
# [step 0] pci 0000:00:05.0: [1234:11e8] type 00 class 0x00ff00 conventional PCI endpoint
# [step 1] pci 0000:00:05.0: BAR 0 [mem 0x00000000-0x000fffff]
# [step 2] pci 0000:00:05.0: BAR 0 [mem 0x20000000-0x200fffff]: assigned

# 0. PCI subsystem identifies the device
# 1. set BAR0 (location 10h) to ffffffff
#    -PCI will return size like fff00000 (1MB)
setpci -d $ID 10.l=ffffffff
RAW=$(setpci -d $ID 10.l)
SIZE=$(( ~0x$RAW & 0xffffffff ))

# 2. PCI subsystem will choose physical address to map
#    -PCI used 0x20000000 and set 'Memory Space enable' (register 04h=2)
PHY_ADDR=20000000
setpci -d $ID 10.l=$PHY_ADDR
setpci -d $ID 04.w=0002

RAW=$(setpci -d $ID 10.l)
printf "Assigned BAR0 to 0x%x (size: 0x%x = 1MB)\n" 0x$RAW $SIZE
