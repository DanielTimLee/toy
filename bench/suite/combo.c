// suite/combo.c

#include <linux/module.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include "../common.h"

#define ENTRY_COUNT 512

struct combo_entry {
	struct list_head list;
	int val;
};

static LIST_HEAD(combo_list);
static DEFINE_SPINLOCK(combo_lock);
static struct combo_entry *entries;

struct torture_stat combo_reader_stat;
struct torture_stat combo_writer_stat;

static void combo_spinlist_init(void)
{
	int i;
	entries = kmalloc_array(ENTRY_COUNT, sizeof(*entries), GFP_KERNEL);
	for (i = 0; i < ENTRY_COUNT; ++i) {
		INIT_LIST_HEAD(&entries[i].list);
		entries[i].val = i;
		list_add_tail(&entries[i].list, &combo_list);
	}
}

static void combo_spinlist_cleanup(void)
{
	INIT_LIST_HEAD(&combo_list);
	kfree(entries);
}

static int combo_reader(void *data)
{
	struct combo_entry *e;
	init_stat(&combo_reader_stat);

	while (!kthread_should_stop()) {
		spin_lock(&combo_lock);
		list_for_each_entry(e, &combo_list, list) {
			e->val += 1;
			atomic64_inc(&combo_reader_stat.op_count);
		}
		spin_unlock(&combo_lock);
	}

	end_stat(&combo_reader_stat);
	return 0;
}

static int combo_writer(void *data)
{
	struct combo_entry *e;
	init_stat(&combo_writer_stat);

	while (!kthread_should_stop()) {
		spin_lock(&combo_lock);
		if (!list_empty(&combo_list)) {
			e = list_first_entry(&combo_list, struct combo_entry,
					     list);
			e->val += 1000;
			list_del(&e->list);
			list_add_tail(&e->list, &combo_list);
			atomic64_inc(&combo_writer_stat.op_count);
		}
		spin_unlock(&combo_lock);
	}

	end_stat(&combo_writer_stat);
	return 0;
}

static struct torture_ops spinlist_combo_test = {
	.name = "spinlock+list",
	.init = combo_spinlist_init,
	.cleanup = combo_spinlist_cleanup,
	.reader_fn = combo_reader,
	.writer_fn = combo_writer,
};

static struct torture_ops *suite3_tests[] = {
	&spinlist_combo_test,
};

struct test_suite_ops suite3_combo_tests = {
	.test_count = ARRAY_SIZE(suite3_tests),
	.tests = suite3_tests,
	.name = "combo",
};
