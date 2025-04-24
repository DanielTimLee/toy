
obj-m += userspace_livepatch.o
all:
    make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
    make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

gcc -o test_program test_program.c
./test_program &

gcc -shared -o patch_function.so patch_function.c

sudo insmod userspace_livepatch.ko target_pid=<pid> target_addr=0x<target_function_address>

nm patch_function.so | grep my_function

gcc -o ulp_trigger ulp_trigger.c
./ulp_trigger 0x<patch_function_address>

sudo rmmod userspace_livepatch
