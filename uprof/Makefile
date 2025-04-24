KERN_BASE ?= v6.14-x86_64
obj-m += uprof.o
obj-m += uprof_rhashtable.o
obj-m += uprof_rhashtable_seq.o

all:
	make -C /git/src/$(KERN_BASE) M=$(PWD) modules
	$(CROSS_COMPILE)gcc -static -g -pg -o ../env/0_infinite_static.o ../env/0_infinite.c

clean:
	make -C /git/src/$(KERN_BASE) M=$(PWD) clean
	rm -f ../env/0_infinite_static.o
