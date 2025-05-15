// suite/structure.c

#include "linux/ktime.h"
#include "linux/random.h"
#include <linux/module.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>
#include "../common.h"

#define ENTRY_CNT 1000

static DEFINE_SPINLOCK(torture_list_lock);
static LIST_HEAD(torture_list);

struct torture_stat list_reader_stat;
struct torture_stat list_writer_stat;

static void init_list(void)
{
	init_data();
	INIT_LIST_HEAD(&torture_list);
	for (int i = 0; i < ENTRY_CNT; ++i) {
		INIT_LIST_HEAD(&entries[i].list);
		list_add_tail(&entries[i].list, &torture_list);
	}
}

static void cleanup_list(void)
{
	struct data_entry *e, *tmp;
	list_for_each_entry_safe(e, tmp, &torture_list, list)
		list_del(&e->list);

	cleanup_data();
}

static void try_update_max(atomic64_t *max, u64 val)
{
	u64 old;
	do {
		old = atomic64_read(max);
		if (val <= old)
			return;
	} while (atomic64_cmpxchg(max, old, val) != old);
}

static int list_reader(void *data)
{
	struct data_entry *e;
	ktime_t start, delta;
	int key;

	init_stat(&list_reader_stat);

	while (!kthread_should_stop()) {
		key = get_random_u32() % ENTRY_CNT;
		start = ktime_get();

		spin_lock(&torture_list_lock);

		list_for_each_entry(e, &torture_list, list) {
			if (e->key == key) {
				delta = ktime_sub(ktime_get(), start);
				atomic64_inc(&list_reader_stat.op_cnt);
				atomic64_add(delta,
					     &list_reader_stat.tot_latency);
				try_update_max(&list_reader_stat.total_lat_max,
					       delta);
			}
		}
	}

	spin_unlock(&torture_list_lock);
	end_stat(&list_reader_stat);
	return 0;
}

static int list_writer(void *data)
{
	init_stat(&list_writer_stat);

	while (!kthread_should_stop()) {
		spin_lock(&torture_list_lock);
		if (!list_empty(&torture_list)) {
			struct data_entry *first = list_first_entry(
				&torture_list, struct data_entry, list);
			list_del(&first->list);
			list_add_tail(&first->list, &torture_list);

			atomic64_inc(&list_writer_stat.op_cnt);
		}
		spin_unlock(&torture_list_lock);
	}

	end_stat(&list_writer_stat);
	return 0;
}

static struct torture_ops list_test = {
	.name = "list_head",
	.init = init_list,
	.cleanup = cleanup_list,
	.reader_fn = list_reader,
	.writer_fn = list_writer,
};

/* 향후 hlist, rhashtable 등 추가 가능 */
struct torture_ops *suite2_tests[] = { &list_test };

struct test_suite_ops suite2_ds_tests = {
	.test_cnt = ARRAY_SIZE(suite2_tests),
	.tests = suite2_tests,
	.name = "datastructure",
};
