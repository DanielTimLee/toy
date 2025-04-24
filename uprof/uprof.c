#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#define PROC_NAME "profile"
#define MAX_ENTRIES 1024

static ulong target_offset = -1; // ELF offset
static struct proc_dir_entry *iface;

struct entry {
	unsigned long addr;
	unsigned long count;
	unsigned long index;
};

static struct uprofile {
	pid_t PID;
	bool ready;
	char *print_buffer;
	unsigned long index;

	struct entry buffer[MAX_ENTRIES];
	unsigned int head;
	unsigned int tail;
	spinlock_t lock;

	struct inode *f_inode;
	struct uprobe *uprobe;
	struct uprobe_consumer consumer;
} uprof = { .ready = false, .consumer.handler = NULL };

static int uprobe_handler(struct uprobe_consumer *self, struct pt_regs *regs,
			  __u64 *data)
{
	unsigned long parent_ip = *(unsigned long *)regs->sp;
	unsigned long flags;

	if (current->pid != uprof.PID)
		return 0;

	spin_lock_irqsave(&uprof.lock, flags);

	// Lookup for same entry
	for (int i = uprof.tail; i != uprof.head; i = (i + 1) % MAX_ENTRIES) {
		if (uprof.buffer[i].addr == parent_ip) {
			uprof.buffer[i].count++;
			goto done;
		}
	}

	uprof.buffer[uprof.head].addr = parent_ip;
	uprof.buffer[uprof.head].count = 1;
	uprof.buffer[uprof.head].index = uprof.index++;
	uprof.head = (uprof.head + 1) % MAX_ENTRIES;

	if (uprof.head == uprof.tail)
		uprof.tail = (uprof.tail + 1) % MAX_ENTRIES; // overwrite oldest

done:
	spin_unlock_irqrestore(&uprof.lock, flags);
	return 0;
}

static void profile_clear(void)
{
	unsigned long flags;

	// Stop uprobe
	if (uprof.uprobe && !IS_ERR(uprof.uprobe)) {
		uprobe_unregister_nosync(uprof.uprobe, &uprof.consumer);
		uprobe_unregister_sync();
		uprof.uprobe = NULL;
	}

	if (uprof.print_buffer)
		memset(uprof.print_buffer, 0, PAGE_SIZE);

	spin_lock_irqsave(&uprof.lock, flags);

	uprof.head = uprof.tail = 0;
	uprof.index = 0;

	spin_unlock_irqrestore(&uprof.lock, flags);
}

static void profile_cleanup(void)
{
	profile_clear();
	kfree(uprof.print_buffer);
}

static int profile_start(void)
{
	struct pid *pid_struct;
	struct task_struct *t;
	struct mm_struct *mm;
	int ret = -EINVAL;

	if (uprof.PID <= 0) {
		pr_err("PID fail\n");
		goto err;
	}

	pid_struct = find_get_pid(uprof.PID);
	if (!pid_struct) {
		pr_err("PID fail\n");
		goto err;
	}

	rcu_read_lock();
	t = pid_task(pid_struct, PIDTYPE_PID);
	if (!t) {
		pr_err("PID->Task fail\n");
		rcu_read_unlock();
		goto err;
	}

	mm = get_task_mm(t);
	rcu_read_unlock();
	if (!mm || !mm->exe_file) {
		pr_err("PID->Task->mm->exe_file fail\n");
		mmput(mm);
		goto err;
	}

	uprof.f_inode = mm->exe_file->f_inode;
	put_pid(pid_struct);
	mmput(mm);

	uprof.uprobe = uprobe_register(uprof.f_inode, target_offset, 0,
				       &uprof.consumer);
	if (IS_ERR(uprof.uprobe)) {
		pr_err("PID uprobe fail\n");
		goto err;
	}

	ret = 0;
err:
	return ret;
}

static ssize_t proc_read(struct file *file, char __user *buf, size_t count,
			 loff_t *pos)
{
	struct entry *p;
	size_t len = 0;

	if (*pos > 0) {
		*pos = 0;
		goto eof;
	}

	len += snprintf(uprof.print_buffer, PAGE_SIZE, "PID: [%d] profiling\n",
			uprof.PID);

	spin_lock(&uprof.lock);
	for (int i = uprof.tail; i != uprof.head && len < PAGE_SIZE - 64;
	     i = (i + 1) % MAX_ENTRIES) {
		p = &uprof.buffer[i];
		len += snprintf(uprof.print_buffer + len, PAGE_SIZE - len,
				"Addr: 0x%lx Count: %lu\n", p->addr, p->count);
	}
	spin_unlock(&uprof.lock);

	if (copy_to_user(buf, uprof.print_buffer, len))
		return -EFAULT;

	*pos += len;
eof:
	return len;
}

static const struct proc_ops proc_fops = {
	.proc_read = proc_read,
};

static int __init profile_init(void)
{
	spin_lock_init(&uprof.lock);
	uprof.consumer.handler = uprobe_handler;

	uprof.print_buffer = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!uprof.print_buffer)
		goto cleanup;

	iface = proc_create(PROC_NAME, 0666, NULL, &proc_fops);
	if (!iface)
		goto cleanup;

	uprof.ready = true;
	profile_clear();
	profile_start();

	return 0;
cleanup:
	profile_cleanup();
	return -EINVAL;
}

static void __exit profile_exit(void)
{
	if (iface)
		proc_remove(iface);

	profile_cleanup();
}

static int PID_set(const char *val, const struct kernel_param *kp)
{
	int ret = param_set_int(val, kp);
	if (ret == 0 && uprof.ready) {
		profile_clear();
		profile_start();
	}

	return ret;
}

static const struct kernel_param_ops p_ops = { .set = PID_set,
					       .get = param_get_int };

module_param_cb(pid, &p_ops, &uprof.PID, 0644);
module_param(target_offset, ulong, 0644);

module_init(profile_init);
module_exit(profile_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Userspace profiling (-pg, -mfentry)");
