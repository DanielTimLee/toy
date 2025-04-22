#!/bin/bash

DRIVER=${1:-3_PCI_driver}
VER=${2:-1.0}
DIR=/usr/src/$DRIVER-$VER

if [[ "$EUID" -ne 0 ]]; then
  echo "This script must be run as root."
  exit 1
fi

if ! ls ${DRIVER}* &>/dev/null; then
    echo "${DRIVER} not found in CWD."
    exit 1
fi

rmmod $DRIVER > /dev/null
dkms uninstall -m $DRIVER -v $VER > /dev/null
dkms remove -m $DRIVER -v $VER > /dev/null
rm -rf $DIR

mkdir -p $DIR
cp -r * $DIR

# Creating dkms.conf
cat <<EOF > $DIR/dkms.conf
PACKAGE_NAME="${DRIVER}"
PACKAGE_VERSION="${VER}"
BUILT_MODULE_NAME[0]="${DRIVER}"
DEST_MODULE_LOCATION[0]="/kernel/drivers/misc"
AUTOINSTALL="yes"
EOF

dkms add -m $DRIVER -v $VER
dkms build -m $DRIVER -v $VER
dkms install -m $DRIVER -v $VER

dkms status
modprobe $DRIVER
lsmod | grep $DRIVER
