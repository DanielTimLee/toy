KERN_BASE ?= v6.14-x86_64
obj-m += bench.o
bench-objs := \
	main.o \
	stat.o \
	test.o \
	suite/lock.o \
	suite/ds.o
	# suite/structure.o \
	# suite/combo.o \
	# suite/scaling.o \
	# suite/benchmark.o

all:
	make -C /git/src/$(KERN_BASE) M=$(PWD) modules

clean:
	make -C /git/src/$(KERN_BASE) M=$(PWD) clean
