#!/bin/bash

if [[ "$EUID" -ne 0 ]]; then
  echo "This script must be run as root."
  exit 1
fi

MODULE=${1:-memmap.ko}

TEST() {
    local ADDR=$1 SIZE=$2
    insmod $MODULE phys_addr=$ADDR phys_size=$SIZE

    memtool md -s /dev/memmap 0+8
    memtool mw -d /dev/memmap 0 0xAABBCCDDAABBCCDD
    memtool md -s /dev/memmap 0+8

    rmmod $MODULE
}

TEST 0x40000000 0x1000
TEST 0x40001000 0x1000
TEST 0x60000000 0x1000
