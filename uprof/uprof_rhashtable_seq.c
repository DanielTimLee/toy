#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/rhashtable.h>

#define PROC_NAME "profile"

static ulong target_offset = -1; // ELF offset
static struct proc_dir_entry *iface;

struct entry {
	unsigned long addr;
	unsigned long count;
	unsigned long index;
	struct rcu_head rcu;
	struct rhash_head node;
};

static struct uprofile {
	pid_t PID;
	bool ready;
	unsigned long index;
	struct rhashtable pt;

	struct inode *f_inode;
	struct uprobe *uprobe;
	struct uprobe_consumer consumer;
} uprof = { .ready = false, .consumer.handler = NULL };

static const struct rhashtable_params ptp = {
	.key_len = sizeof(unsigned long),
	.key_offset = offsetof(struct entry, addr),
	.head_offset = offsetof(struct entry, node),
	.automatic_shrinking = true,
};

static void entry_rcu_free(struct rcu_head *rcu)
{
	kfree(container_of(rcu, struct entry, rcu));
}

static int uprobe_handler(struct uprobe_consumer *self, struct pt_regs *regs,
			  __u64 *data)
{
	unsigned long parent_ip = *(unsigned long *)regs->sp;
	struct entry *new, *old;

	if (current->pid != uprof.PID)
		return 0;

	new = kzalloc(sizeof(struct entry), GFP_KERNEL);
	if (!new)
		return 0;

	new->addr = parent_ip;
	new->count = 1;
	new->index = uprof.index++;

	old = rhashtable_lookup_get_insert_fast(&uprof.pt, &new->node, ptp);
	if (old) {
		kfree(new);
		old->count++;
		uprof.index--;
	}

	return 0;
}

static void profile_clear(void)
{
	struct rhashtable_iter iter;
	struct entry *p;

	// Stop uprobe
	if (uprof.uprobe && !IS_ERR(uprof.uprobe)) {
		uprobe_unregister_nosync(uprof.uprobe, &uprof.consumer);
		uprobe_unregister_sync();
		uprof.uprobe = NULL;
	}

	rhashtable_walk_enter(&uprof.pt, &iter);
	do {
		rhashtable_walk_start(&iter);
		while ((p = rhashtable_walk_next(&iter)) && !IS_ERR(p)) {
			if (!rhashtable_remove_fast(&uprof.pt, &p->node, ptp))
				call_rcu(&p->rcu, entry_rcu_free);
		}
		rhashtable_walk_stop(&iter);
	} while (p == ERR_PTR(-EAGAIN));

	rhashtable_walk_exit(&iter);

	uprof.index = 0;
}

static void profile_cleanup(void)
{
	profile_clear();
	rhashtable_destroy(&uprof.pt);
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

static void *profile_seq_start(struct seq_file *m, loff_t *pos)
{
	struct rhashtable_iter *iter = m->private;
	void *p;

	if (!iter)
		return NULL;

	rcu_read_lock();
	rhashtable_walk_enter(&uprof.pt, iter);
	rhashtable_walk_start(iter);

	for (loff_t i = 0; i <= *pos; i++) {
		p = rhashtable_walk_next(iter);
		if (IS_ERR(p) || !p)
			return NULL;
	}

	return p;
}

static int profile_seq_show(struct seq_file *m, void *v)
{
	struct entry *e = v;

	if (m->index == 0)
		seq_printf(m, "PID: [%d] profiling\n", uprof.PID);

	seq_printf(m, "Addr: 0x%lx Count: %lu\n", e->addr, e->count);
	return 0;
}

static void *profile_seq_next(struct seq_file *m, void *v, loff_t *pos)
{
	struct rhashtable_iter *iter = m->private;
	void *p;

	(*pos)++;
	p = rhashtable_walk_next(iter);
	if (IS_ERR(p) || !p)
		return NULL;

	return p;
}

static void profile_seq_stop(struct seq_file *m, void *v)
{
	struct rhashtable_iter *iter = m->private;
	if (iter) {
		rhashtable_walk_stop(iter);
		rhashtable_walk_exit(iter);
		rcu_read_unlock();
	}
}

// seq_open() → seq_read() → [start → show → next → show → ... → stop] → seq_release()
static const struct seq_operations profile_seq_ops = {
	.start = profile_seq_start,
	.show = profile_seq_show,
	.next = profile_seq_next,
	.stop = profile_seq_stop,
};

static int profiler_open(struct inode *inode, struct file *file)
{
	return seq_open_private(file, &profile_seq_ops,
				sizeof(struct rhashtable_iter));
}

static const struct proc_ops proc_fops = {
	.proc_open = profiler_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};

static int __init profile_init(void)
{
	uprof.consumer.handler = uprobe_handler;

	if (rhashtable_init(&uprof.pt, &ptp))
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
