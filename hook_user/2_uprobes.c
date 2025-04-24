#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uprobes.h>
#include <linux/uaccess.h>
#include <linux/pid.h>
#include <linux/mm.h>
#include <linux/fs.h>

static pid_t pid = 0;
static ulong target_offset = -1; // ELF offset
static char *fname = "/tmp/from_kernel.txt";
static char *fcontent = "Hello from uprobe!";

module_param(pid, int, 0644);
module_param(fname, charp, 0644);
module_param(fcontent, charp, 0644);
module_param(target_offset, ulong, 0644);

static int handler_pre(struct uprobe_consumer *self, struct pt_regs *regs,
		       __u64 *data)
{
	struct file *f;
	ssize_t result;
	loff_t pos = 0;

	f = filp_open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (IS_ERR(f)) {
		pr_err("Failed to open %s\n", fname);
		return 0;
	}

	result = kernel_write(f, fcontent, strlen(fcontent), &pos);
	if (result < 0) {
		pr_err("Failed to write to %s\n", fname);
	}

	filp_close(f, NULL);

	pr_info("Write success");
	return 0;
}

struct _up {
	struct inode *f_inode;
	struct uprobe *uprobe;
	struct uprobe_consumer consumer;
};

static struct _up my_up = { .consumer.handler = handler_pre };

static int __init up_init(void)
{
	struct task_struct *target_task;
	struct pid *pid_struct;
	struct mm_struct *mm;
	int ret = -1;

	if (pid <= 0 || target_offset <= 0) {
		pr_err("Provide PID and offset\n");
		goto cleanup;
	}

	pr_err("Register Uprobes at PID %d Offset 0x%lx\n", pid, target_offset);
	// beware of ref count for pid
	pid_struct = find_get_pid(pid);
	if (!pid_struct) {
		pr_err("PID: %d Not found\n", pid);
		goto cleanup;
	}

	// Notify to stop modify RCU since reading task
	rcu_read_lock();
	target_task = pid_task(pid_struct, PIDTYPE_PID);
	if (!target_task) {
		rcu_read_unlock();
		pr_err("PID->Task fail\n");
		goto cleanup;
	}

	// mm lock handled within get_task_mm()
	// let's hold RCU lock until fetching mm. (prevent from mm change)
	mm = get_task_mm(target_task);
	rcu_read_unlock();

	if (!mm || !mm->exe_file) {
		pr_err("PID->Task->mm->exe_file fail\n");
		if (mm)
			mmput(mm);
		goto cleanup;
	}

	my_up.f_inode = mm->exe_file->f_inode;

	// Do not release ref count(mm, pid) inside RCU lock
	// RCU should not be sleeped due to other lock release
	put_pid(pid_struct);
	mmput(mm);

	my_up.uprobe = uprobe_register(my_up.f_inode, target_offset, 0,
				       &my_up.consumer);
	if (IS_ERR(my_up.uprobe)) {
		pr_err("Failed to allocate uprobe");
		goto cleanup;
	}

	ret = 0;
cleanup:
	return ret;
}

static void __exit up_exit(void)
{
	if (my_up.uprobe && !IS_ERR(my_up.uprobe)) {
	    uprobe_unregister_nosync(my_up.uprobe, &my_up.consumer);
	    uprobe_unregister_sync();
	    my_up.uprobe = NULL;
	}
}

module_init(up_init);
module_exit(up_exit);
MODULE_LICENSE("GPL");
