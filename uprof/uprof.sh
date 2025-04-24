#!/bin/bash

if [[ "$EUID" -ne 0 ]]; then
  echo "This script must be run as root."
  exit 1
fi

SYMBOL="mcount"
MODULE=${1:-uprof.ko}
PROGRAM="../env/0_infinite_static.o"

$PROGRAM > /dev/null &
PID=$!
sleep 1

get_symbol_offset() {
    local prog=$1 sym=$2 offset

    # Check binary is PIE
    if readelf -h $prog | grep -q 'Type:.*DYN'; then
        offset=0x$(nm $prog | awk -v s=$sym '$3 == s { print $1 }')
    else
        local symbol_va text_va text_offset

        symbol_va=$(( 0x$(nm $prog | awk -v s=$sym '$3 == s { print $1 }') ))
        text_va=$(( 0x$(readelf -S $prog | awk '/.text/ { print $(NF-1) }') ))
        text_offset=$(( 0x$(readelf -S $prog | awk '/.text/ { print $(NF) }') ))

        offset=0x$(printf "%x" $(( symbol_va - text_va + text_offset )))
    fi

    echo $offset
}
OFFSET=$(get_symbol_offset $PROGRAM $SYMBOL)

echo "========================================="
printf "%s %s is located at 0x%x\n" $PROGRAM $SYMBOL $OFFSET
echo "========================================="
insmod $MODULE pid=$PID target_offset=$OFFSET

timeout --foreground 3s \
    watch -n 0.1 "
        awk '/Addr:/ {
            cmd=\"addr2line -f -e $PROGRAM \"\$2;
            cmd | getline f;
            close(cmd);
            print gensub(/(Addr: [^ ]+)/, \"\\\\1,\" f, 1);
            next;
        } 1' /proc/profile
    "

kill $PID

rmmod $MODULE
