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

PROGRAM="../env/0_simple.o"
FILE_NAME="/tmp/hooking.txt"
FILE_CONTENT="userspace_hooked"

cat > /tmp/gdb_script.gdb <<EOF
set \$fd = creat("$FILE_NAME", 0644)
call write(\$fd, "$FILE_CONTENT", ${#FILE_CONTENT})
call close(\$fd)
detach
quit
EOF

if [ -f "$FILE_NAME" ]; then
  echo "Deleting existing file: $FILE_NAME"
  rm "$FILE_NAME"
fi

$PROGRAM &
PID=$!
sleep 1

echo "========================================="
gdb -p $PID -batch -x /tmp/gdb_script.gdb
echo "========================================="
if [ -f "$FILE_NAME" ]; then
    echo "Created file '$FILE_NAME' with content:"
    cat $FILE_NAME
    echo ""
else
    echo "Error: Failed to create file '$FILE_NAME' via gdb."
fi

kill $PID
