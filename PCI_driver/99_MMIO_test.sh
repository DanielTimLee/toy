#!/bin/bash
BASE=$1
DEV=$2

READ="memtool md"
WRITE="memtool mw"

printf "BAR0 is mapped to 0x${BASE}"
if [ -n "$DEV" ]; then
    printf " at ${DEV}"
    READ="memtool md -s $DEV"
    WRITE="memtool mw -d $DEV"
fi
printf "\n\n"

# Used '', since 'devmem2' doesn't support 4 byte read anymore.
# https://github.com/radii/devmem2/blob/master/devmem2.c#L88C1-L89C1

echo "# 0x00 (RO) : identification (0x010000edu)"

ADDR=$(printf "%X" $((0x$BASE + 0x0)))
$READ 0x$ADDR+4                        # fea00000: 010000ed (Magic number)

echo "# 0x04 (RW) : value inversion (~0x00000000 ~0x000000ff)"

ADDR=$(printf "%X" $((0x$BASE + 0x4)))
$WRITE 0x$ADDR      0x00000000
$READ  0x$ADDR+4                      # fea00004: ffffffff (Negate of 0x00000000)
$WRITE 0x$ADDR      0x000000FF
$READ  0x$ADDR+4                      # fea00004: ffffff00 (Negate of 0x000000FF)

echo "# 0x08 (RW) : factorial computation (4! = 24 / 8! = 40320)"

FACT_ADDR=$(printf "%X" $((0x$BASE + 0x8)))
$WRITE 0x$FACT_ADDR 0x00000004
$READ  0x$FACT_ADDR+4                 # fea00008: 00000018 (4! = 24 0x18)
$WRITE 0x$FACT_ADDR 0x00000008
$READ  0x$FACT_ADDR+4                 # fea00008: 00009D80 (8! = 40320 0x9D80)

echo "# 0x20 (RW) : status register (0x01 = WIP, 0x80 = raise interrupt)"
echo "# 0x24 (RO) : interrupt status register (0x01 = interrupt raised)"

STATUS_ADDR=$(printf "%X" $((0x$BASE + 0x20)))
INTERRUPT_ADDR=$(printf "%X" $((0x$BASE + 0x24)))
$WRITE 0x$STATUS_ADDR    0x00000080     # request raise interrupt when done.
$WRITE 0x$FACT_ADDR      10000000       # 10,000,000
$READ  0x$STATUS_ADDR+4                 # fea00020: 00000081 (0x01 + 0x80: still running)
sleep 1
$READ  0x$STATUS_ADDR+4                 # fea00020: 00000080 (0x80)
$READ  0x$INTERRUPT_ADDR+4              # fea00024: 00000001 (interrupt raised)
$READ  0x$FACT_ADDR+4                   # fea00008: 00000000 (overflowed)

echo "# 0x60 (WO) : interrupt raise register (set)"
echo "# 0x64 (WO) : interrupt acknowledge register (clear)"

MANUAL_INT_OFF_ADDR=$(printf "%X" $((0x$BASE + 0x64)))
MANUAL_INT_ON_ADDR=$(printf "%X" $((0x$BASE + 0x60)))
$READ  0x$INTERRUPT_ADDR+4              # fea00024: 00000001 (interrupt still on)
$WRITE 0x$MANUAL_INT_OFF_ADDR 0x00000001  # manual interrupt off
$READ  0x$INTERRUPT_ADDR+4              # fea00024: 00000000 (cleared)
$WRITE 0x$MANUAL_INT_ON_ADDR  0x00000001  # manual interrupt on
$READ  0x$INTERRUPT_ADDR+4              # fea00024: 00000001 (interrupt set)

echo ""
