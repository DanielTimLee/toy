KERN_BASE ?= v6.14-x86_64
obj-m += keylogger.o
obj-m += keylogger_livepatch.o

all:
	make -C /git/src/$(KERN_BASE) M=$(PWD) modules

clean:
	make -C /git/src/$(KERN_BASE) M=$(PWD) clean
