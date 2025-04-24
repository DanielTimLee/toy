KERN_BASE ?= v6.14-x86_64
obj-m += 3_PCI_driver.o
obj-m += 4_PCI_driver_dma.o

all:
	make -C /git/src/$(KERN_BASE) M=$(PWD) modules

clean:
	make -C /git/src/$(KERN_BASE) M=$(PWD) clean
	rm -rf *.bin
