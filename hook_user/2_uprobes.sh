#!/bin/bash
#
# Change behavior of userspace program with GDB (ptrace).
#
# (gdb) p creat("/tmp/hooking.txt", 0644)
# $1 = 3
# (gdb) p write($1, "Hello from hooking!\n", 16)
# $2 = 16
# (gdb) p close($1)
# $3 = 0

if [[ "$EUID" -ne 0 ]]; then
  echo "This script must be run as root."
  exit 1
fi

SYMBOL="tgt_func"
PROGRAM="../env/0_simple.o"
FILE_NAME="/tmp/hooking.txt"
FILE_CONTENT="userspace_hooked"

if [ -f "$FILE_NAME" ]; then
  echo "Deleting existing file: $FILE_NAME"
  rm "$FILE_NAME"
fi

$PROGRAM &
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
insmod 2_uprobes.ko pid=$PID target_offset=$OFFSET \
                    fname=$FILE_NAME fcontent="$FILE_CONTENT"
sleep 3
rmmod 2_uprobes.ko

echo "========================================="
if [ -f "$FILE_NAME" ]; then
    echo "Created file '$FILE_NAME' with content:"
    cat $FILE_NAME
    echo ""
else
    echo "Error: Failed to create file '$FILE_NAME' via uprobes."
fi

kill $PID
