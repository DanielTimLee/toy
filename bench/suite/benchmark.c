// suite/benchmark.c

#include <linux/module.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include "../common.h"

extern struct torture_ops spinlock_test;

static struct perf_event *perf_ctx_switch = NULL;

static void perf_event_setup(void)
{
	struct perf_event_attr attr = {
		.type = PERF_TYPE_SOFTWARE,
		.config = PERF_COUNT_SW_CONTEXT_SWITCHES,
		.size = sizeof(struct perf_event_attr),
		.pinned = 1,
		.disabled = 0,
	};

	perf_ctx_switch =
		perf_event_create_kernel_counter(&attr, -1, NULL, NULL, NULL);

	if (IS_ERR(perf_ctx_switch)) {
		pr_warn("Failed to create perf counter\n");
		perf_ctx_switch = NULL;
	}
}

static u64 read_perf(struct perf_event *event)
{
	u64 count = 0;
	if (event && event->pmu)
		perf_event_read_value(event, &count, NULL);
	return count;
}

static int perf_suite_runner(struct torture_ops *unused, int r, int w)
{
	perf_event_setup();
	u64 before = read_perf(perf_ctx_switch);

	run_torture_threads(&spinlock_test, r, w, 1000); // 1 second test

	u64 after = read_perf(perf_ctx_switch);
	pr_info("[perf] ctx switches for %s: %llu\n", "spinlock",
		after - before);

	if (perf_ctx_switch) {
		perf_event_release_kernel(perf_ctx_switch);
		perf_ctx_switch = NULL;
	}

	return 0;
}

static struct torture_ops perf_spinlock_test = {
	.name = "perf:spinlock",
	.run_fn = perf_suite_runner,
};

static struct torture_ops *suite5_tests[] = {
	&perf_spinlock_test,
};

struct test_suite_ops suite5_perf_tests = {
	.test_count = ARRAY_SIZE(suite5_tests),
	.tests = suite5_tests,
	.name = "perf",
};
