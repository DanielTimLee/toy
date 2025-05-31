// bench: benchmark framework for kernel locks and data structures

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include "common.h"

int entry_cnt = 1000;
int log_level = LOG_INFO;
static int reader_cnt = 4;
static int writer_cnt = 2;
static int runner_cnt = 1;
static int duration_ms = 3000; // ms

module_param(entry_cnt, int, 0644);
module_param(log_level, int, 0644);
module_param(runner_cnt, int, 0644);
module_param(reader_cnt, int, 0644);
module_param(writer_cnt, int, 0644);
module_param(duration_ms, int, 0644);

extern struct bench_suite suite1_lock_tests;
extern struct bench_suite suite2_ds_tests;
struct bench_suite *suites[] = { &suite1_lock_tests, &suite2_ds_tests };
/* struct bench_suite *suites[] = { &suite2_ds_tests }; */

static void bench_single_test(struct bench_test *test)
{
	log_info("Starting %s test (%d ms, %d entry, Runner: %d)\n", test->name,
		 duration_ms, entry_cnt, runner_cnt);

	run_single_test(test, duration_ms, runner_cnt);
}

static void bench_parallel_test(struct bench_test *test)
{
	log_info("Starting %s test (%d ms, Reader / Writer: %d / %d)\n",
		 test->name, duration_ms, reader_cnt, writer_cnt);

	run_parallel_test(test, duration_ms, reader_cnt, writer_cnt);
}

static void bench_run(struct bench_test *test)
{
	if (test->runner)
		bench_single_test(test);
	else if (test->reader || test->writer)
		bench_parallel_test(test);
}

static int __init bench_init(void)
{
	log_info("Kernel bench init\n");
	overhead_test();

	for (int s = 0; s < ARRAY_SIZE(suites); ++s) {
		struct bench_suite *suite = suites[s];
		log_info("== Suite: %s ==\n", suite->name);

		for (int t = 0; t < suite->test_cnt; ++t)
			bench_run(suite->tests[t]);
	}

	return 0;
}

static void __exit bench_exit(void) { log_info("Kernel bench exit\n"); }

module_init(bench_init);
module_exit(bench_exit);

MODULE_LICENSE("GPL");
