// suite/scaling.c

#include <linux/module.h>
#include "../common.h"

extern struct torture_ops spinlock_test;

static struct torture_stat scaling_stat;

static int scaling_runner(struct torture_ops *base, int max_r, int max_w)
{
	int r, w;
	pr_info("== Scaling Test for: %s ==\n", base->name);

	for (r = 1; r <= max_r; r *= 2) {
		for (w = 1; w <= max_w; w *= 2) {
			pr_info("[scaling] %d readers / %d writers\n", r, w);
			init_stat(&scaling_stat);

			base->init();
			run_torture_threads(base, r, w, 3000); // 3초 실행
			base->cleanup();

			end_stat(&scaling_stat);
			pr_info("[scaling][%s] %dR/%dW throughput: ",
				base->name, r, w);
			report_stat(base->name, &scaling_stat);
		}
	}
	return 0;
}

static int scaling_spinlock_runner(struct torture_ops *unused, int r, int w)
{
	return scaling_runner(&spinlock_test, r, w);
}

static struct torture_ops scaling_spinlock_test = {
	.name = "scaling:spinlock",
	.init = NULL,
	.cleanup = NULL,
	.run_fn = scaling_spinlock_runner,
};

static struct torture_ops *suite4_tests[] = {
	&scaling_spinlock_test,
};

struct test_suite_ops suite4_scaling_tests = {
	.test_cnt = ARRAY_SIZE(suite4_tests),
	.tests = suite4_tests,
	.name = "scaling",
};
