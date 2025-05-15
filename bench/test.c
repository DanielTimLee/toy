// test.c: bench test runner

#include <linux/hrtimer.h>
#include <linux/kthread.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/sched/signal.h>
#include <linux/sched.h>
#include "common.h"

enum bench_flags { BENCH_START = BIT(1), BENCH_STOP = BIT(2) };
atomic_t bench_ctrl = ATOMIC_INIT(0);

struct data_entry *entries[ENTRY_CNT];
struct kmem_cache *entry_cache;

static int do_bench_test(void *kthread_data)
{
	struct bench_runner *ctx = kthread_data;
	struct bench_stat *stat = &ctx->stat;
	struct bench_unit *unit = ctx->unit;
	void *priv = unit->priv;
	ktime_t t0, t1, t2, t3;

	while (!(atomic_read(&bench_ctrl) & BENCH_START))
		cpu_relax(); // or msleep(1)

	while (!(atomic_read(&bench_ctrl) & BENCH_STOP)) {
		t0 = ktime_get();

		unit->begin(priv);

		t1 = ktime_get();

		unit->apply(priv);

		t2 = ktime_get();

		unit->clean(priv);

		t3 = ktime_get();

		collect_latency(&stat->begin, ktime_sub(t1, t0));
		collect_latency(&stat->apply, ktime_sub(t2, t1));
		collect_latency(&stat->clean, ktime_sub(t3, t2));
		collect_latency(&stat->total, ktime_sub(t3, t0));

		stat->op_cnt++;
	}

	complete(&ctx->done);
	return 0;
}

static int bench_runner_init(struct bench_group *grp, struct bench_unit *unit)
{
	grp->ctxs = kcalloc(grp->count, sizeof(*grp->ctxs), GFP_KERNEL);
	grp->threads = kcalloc(grp->count, sizeof(*grp->threads), GFP_KERNEL);

	if (!grp->ctxs || !grp->threads)
		return -ENOMEM;

	for (int i = 0; i < grp->count; i++) {
		struct bench_runner *ctx = &grp->ctxs[i];

		snprintf(ctx->name, sizeof(ctx->name), "%s%d", grp->name, i);
		ctx->stat = init_bench_stat(unit, ctx->name);
		init_completion(&ctx->done);
		ctx->unit = unit;
	}

	return 0;
}

static void bench_runner_free(struct bench_group *grp)
{
	kfree(grp->ctxs);
	kfree(grp->threads);
}

static int bench_runner_start(struct bench_group *grp)
{
	for (int i = 0; i < grp->count; i++) {
		grp->threads[i] = kthread_run(&do_bench_test, &grp->ctxs[i],
					      "%s", grp->ctxs[i].name);

		if (IS_ERR(grp->threads[i]))
			return PTR_ERR(grp->threads[i]);
	}

	return 0;
}

static void bench_runner_wait_and_collect(struct bench_group *grp)
{
	for (int i = 0; i < grp->count; i++) {
		wait_for_completion(&grp->ctxs[i].done);
		reduce_bench_stat(grp->stat, &grp->ctxs[i].stat);

		if (log_level >= LOG_DEBUG)
			report_stat(&grp->ctxs[i].stat);
	}
}

static int run_generic_bench(struct bench_test *test, struct bench_group **grps,
			     int group_cnt, int duration_ms)
{
	ktime_t start, end, until = ktime_set(0, duration_ms * NSEC_PER_MSEC);
	atomic_set(&bench_ctrl, 0);

	if (test->init())
		return -EINVAL;

	// 1. group init and create kthread
	for (int i = 0; i < group_cnt; i++)
		if (bench_runner_init(grps[i], grps[i]->unit) ||
		    bench_runner_start(grps[i]))
			goto cleanup;

	// 2. Start/Stop threads
	start = ktime_get();
	atomic_set(&bench_ctrl, BENCH_START);

	// Normally, WQ should be defined and push the task to WQ to wake later.
	// (Task will be removed from ready queue and moved to wait queue.)
	// But here task stays on ready queue, and kernel arms TIMER and wakes up.
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_hrtimeout(&until, HRTIMER_MODE_REL);
	// or msleep(duration_ms);

	atomic_set(&bench_ctrl, BENCH_STOP);
	end = ktime_get();

	// 3. stat aggegate and cleanup
	for (int i = 0; i < group_cnt; i++) {
		grps[i]->stat->start = start;
		grps[i]->stat->end = end;
		bench_runner_wait_and_collect(grps[i]);
	}

cleanup:
	for (int i = 0; i < group_cnt; i++)
		bench_runner_free(grps[i]);

	test->cleanup();
	return 0;
}

int run_single_test(struct bench_test *test, int duration_ms, int runner_cnt)
{
	struct bench_unit *unit = test->runner;
	struct bench_group *grps[1];
	struct bench_stat stat;

	INIT_BENCH_GROUP(grp, runner_cnt, unit, &stat, "%s_T", test->name);
	grps[0] = &grp;

	run_generic_bench(test, grps, 1, duration_ms);
	report_stat(&stat);

	return 0;
}

int run_parallel_test(struct bench_test *test, int duration_ms, int reader_cnt,
		      int writer_cnt)
{
	struct bench_unit *r_unit = test->reader;
	struct bench_unit *w_unit = test->writer;
	struct bench_group *grps[2];
	struct bench_stat rstat;
	struct bench_stat wstat;

	INIT_BENCH_GROUP(r_grp, reader_cnt, r_unit, &rstat, "%s_R", test->name);
	INIT_BENCH_GROUP(w_grp, writer_cnt, w_unit, &wstat, "%s_W", test->name);
	grps[0] = &r_grp;
	grps[1] = &w_grp;

	run_generic_bench(test, grps, 2, duration_ms);
	report_stat(&rstat);
	report_stat(&wstat);

	return 0;
}

inline void fn_write(void *arg) { *(char *)arg = 0; }
inline void fn_ktime(void *arg) { /* ktime_get(); */ }
inline void fn_atomic64(void *arg) { atomic64_read((atomic64_t *)arg); }

static void measure_overhead(struct lat_stat *stat, int iter,
			     void (*fn)(void *), void *arg)
{
	ktime_t t1, t2;
	for (int i = 0; i < iter; i++) {
		t1 = ktime_get();
		fn(arg);
		t2 = ktime_get();

		collect_latency(stat, ktime_sub(t2, t1));
	}

	print_latency(stat, iter);
}

void overhead_test(void)
{
	void *ptr = vmalloc(PAGE_SIZE);
	INIT_LAT_STAT(pf_stat, "page_fault");
	INIT_LAT_STAT(ktime_stat, "ktime_get");
	INIT_LAT_STAT(atomic_stat, "atomic_read");

	measure_overhead(&pf_stat, 1, fn_write, ptr);
	measure_overhead(&ktime_stat, 100000, fn_ktime, NULL);
	measure_overhead(&atomic_stat, 100000, fn_atomic64, &bench_ctrl);

	vfree(ptr);
}

void init_cache(void)
{
	entry_cache = kmem_cache_create("entry_cache",
					sizeof(struct data_entry),
					__alignof__(struct data_entry),
					SLAB_HWCACHE_ALIGN, NULL);
}

void init_entries(void)
{
	for (int i = 0; i < ENTRY_CNT; ++i) {
		entries[i] = kmem_cache_zalloc(entry_cache, GFP_KERNEL);
		entries[i]->key = i;
	}
}

void cleanup_entries(void)
{
	for (int i = 0; i < ENTRY_CNT; ++i) {
		if (entries[i]) {
			kmem_cache_free(entry_cache, entries[i]);
			entries[i] = NULL;
		}
	}
}

void cleanup_cache(void)
{
	kmem_cache_destroy(entry_cache);
	entry_cache = NULL;
}
