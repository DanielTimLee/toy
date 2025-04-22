KERN_BASE ?= v6.14-x86_64
obj-m += syscall_hook.o
obj-m += syscall_hook_v6_9+.o

all:
	make -C /git/src/$(KERN_BASE) M=$(PWD) modules
clean:
	make -C /git/src/$(KERN_BASE) M=$(PWD) clean
