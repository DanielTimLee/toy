KERN_BASE ?= v6.14-aarch64
obj-m += memmap.o

all:
	make -C /git/src/$(KERN_BASE) M=$(PWD) modules
	$(CROSS_COMPILE)gcc -o user.o user.c

clean:
	make -C /git/src/$(KERN_BASE) M=$(PWD) clean
	rm -rf user
