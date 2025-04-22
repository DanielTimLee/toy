#!/bin/bash

source ./99_env.sh "$@"

sudo mkdir -p /git/src
sudo chown -R $USER:$USER /git

sudo apt install zstd bear wget \
    build-essential pahole kmod flex bison bc libncurses-dev \
    libelf-dev libssl-dev silversearcher-ag universal-ctags \
    debootstrap mmdebstrap $APT_EXTRA -y

if [[ ! -d "/git/net-next" ]]; then
    cd /git
    git clone git://git.kernel.org/pub/scm/linux/kernel/git/netdev/net-next.git
fi

if [[ ! -d "/git/src/$KERN_BASE" ]]; then
    cd /git/net-next
    git worktree add -B $KERN_BASE /git/src/$KERN_BASE $KERN_VER
fi

cd /git/src/$KERN_BASE

if [[ ! -f .config ]]; then
    cat tools/testing/selftests/bpf/config > .config
    cat tools/testing/selftests/bpf/config.vm >> .config
    cat tools/testing/selftests/bpf/config.$TARGET >> .config

    if [[ "$TARGET" == "x86_64" ]]; then
        ./scripts/config --disable  CONFIG_X86_KERNEL_IBT
        ./scripts/config --enable   CONFIG_HYPERVISOR_GUEST
        ./scripts/config --enable   CONFIG_PARAVIRT
        ./scripts/config --enable   CONFIG_KVM_GUEST
    fi
    ./scripts/config --disable  CONFIG_RANDOMIZE_BASE
    ./scripts/config --enable   CONFIG_DYNAMIC_DEBUG
    ./scripts/config --enable   CONFIG_LIVEPATCH
    ./scripts/config --enable   CONFIG_KGDB
    ./scripts/config --module   CONFIG_UIO
    ./scripts/config --module   CONFIG_UIO_PDRV_GENIRQ
    ./scripts/config --module   CONFIG_UIO_PCI_GENERIC
fi

make olddefconfig
bear -- make
