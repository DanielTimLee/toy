KERN_BASE ?= v6.14-x86_64
obj-m += 2_uprobes.o

all:
	make -C /git/src/$(KERN_BASE) M=$(PWD) modules
	$(CROSS_COMPILE)gcc -g -pg -o ../env/0_simple.o ../env/0_simple.c

clean:
	make -C /git/src/$(KERN_BASE) M=$(PWD) clean
	rm -f ../env/0_simple.o
