# toy

- raw_zlib_compress/
- keylogger/
- hook_syscall/
- hook_userspace/
- profiler/
- PCI_driver/
- memmap/
- bench/

## Run

use `env/` scripts for qemu
- 1. run 1_kernel.sh to build kernel image
- 2. run 2_rootfs.sh to build qemu disk image
- 3. run 3_qemu.sh to start qemu

##### QEMU debug(-s) vs KGDB

QEMU Debug: QEMU GDB Stub
- Entire guest system (bootloader, kernel..)
- No extra shell

KGDB: Kernel Integrated debugging framework (CONFIG_KGDB)
- Only kernel
- provides "kdb" shell
- cmdline += "kgdbwait kgdboc=ttyS0,115200"

##### Useful Commands

```
# check module .text address (pass to add-symbol-file <.ko> <addr>
$ cat /proc/modules

# run code not exists in binary
$ gdb -ex p creat("/tmp/txt")

# compare every character
$ cmp <file1> <file2>

```


##### IDEAS

adaptive lock
bench framework

ipmr_cache_free_rcu
net/ipv4/ipmr.c:741:    call_rcu(&c->_c.rcu, ipmr_cache_free_rcu);
net/ipv6/ip6mr.c:793:   call_rcu(&c->_c.rcu, ip6mr_cache_free_rcu);


can be cleared with kfree


#### BUG?

```
#define TEST_OPS(name, INIT_FN, CLEANUP_FN) \
struct bench_ops name##_test = {    \
	.name = #name,              \         // #name NOT resolved as "string"
	.init = INIT_FN,            \         // change name to NAME resolves problem
	.cleanup = CLEANUP_FN,      \
	.reader_fn = NAME##_reader, \
	.writer_fn = NAME##_writer, \
}
```
