// common.h
#ifndef _BENCH_COMMON_H_
#define _BENCH_COMMON_H_

#include <linux/completion.h>
#include <linux/ktime.h>

#define NAME_LEN 32

extern int log_level;
enum LOG_LEVEL { LOG_INFO = 0, LOG_DEBUG, LOG_VERBOSE };

#define LOG(level, fmt, ...)                         \
	do {                                         \
		if (log_level >= (level))            \
			pr_info(fmt, ##__VA_ARGS__); \
	} while (0)

#define log_info(fmt, ...) LOG(LOG_INFO, fmt, ##__VA_ARGS__)
#define log_debug(fmt, ...) LOG(LOG_DEBUG, fmt, ##__VA_ARGS__)
#define log_verbose(fmt, ...) LOG(LOG_VERBOSE, fmt, ##__VA_ARGS__)

#define ILOG_BASE 5 // skip 0~(2^5)
#define BUCKETS 16 // 2^5 ~ 2^21 (2 097 152 ns = 2ms)

struct lat_stat {
	u64 min, max, tot;
	u32 hist[BUCKETS];
	const char *name;
	// 96B = 64B Cache Line * 1.5
};

struct bench_stat {
	struct lat_stat begin;
	struct lat_stat apply;
	struct lat_stat clean;
	struct lat_stat total;
	u64 op_cnt;
	char *name;

	ktime_t start;
	ktime_t end;
};

struct bench_result {
	struct bench_stat reader;
	struct bench_stat writer;
	struct bench_stat runner;
};

// BENCH : task < test < suite
enum bench_unit_type { BENCH_LOCK };
struct bench_unit {
	enum bench_unit_type type;
	void *priv;

	void (*begin)(void *priv);
	void (*apply)(void *priv);
	void (*clean)(void *priv);
};

struct bench_test {
	const char *name;
	int (*init)(void);
	void (*cleanup)(void);

	struct bench_unit *reader;
	struct bench_unit *writer;
	struct bench_unit *runner;
};

struct bench_suite {
	int test_cnt;
	const char *name;
	struct bench_test **tests;
};

// WORKER: runner < group
struct bench_runner {
	struct bench_unit *unit;
	struct bench_stat stat;
	struct completion done;
	// additional fields
	char name[NAME_LEN];
	void *priv;
	int flag;
};

struct bench_group {
	struct task_struct **threads;
	struct bench_runner *ctxs;
	struct bench_unit *unit;
	struct bench_stat *stat;
	char name[NAME_LEN];
	int count;
};

// test.c
int run_single_test(struct bench_test *test, int duration_ms, int runner_cnt);
int run_parallel_test(struct bench_test *test, int duration_ms, int reader_cnt,
		      int writer_cnt);
void overhead_test(void);

#define ENTRY_CNT 1000

struct data_entry {
	struct list_head list;
	struct rcu_head rcu; // RCU async cleanup
	void *priv;
	u64 key;
	// 64 - (16 + 16 + 8) = 24B padding inserted
	u64 val __aligned(64);
} __aligned(64);

void init_cache(void);
void init_entries(void);
void cleanup_entries(void);
void cleanup_cache(void);

// stat.c
struct bench_stat init_bench_stat(struct bench_unit *unit, char *_name);
inline void collect_latency(struct lat_stat *stat, u64 lat);
void reduce_bench_stat(struct bench_stat *to, struct bench_stat *from);
void print_latency(struct lat_stat *stat, u64 cnt);
void report_stat(struct bench_stat *stat);

// clang-format off
#define INIT_BENCH_GROUP(NAME, CNT, UNIT, STAT, ...)         \
	struct bench_group NAME = { .count = CNT,            \
				    .unit = UNIT,            \
				    .stat = STAT };          \
	snprintf(NAME.name, sizeof(NAME.name), __VA_ARGS__); \
	*(STAT) = init_bench_stat(UNIT, NAME.name);

#define LAT_STAT_INIT(_name) { ~0ULL, 0, 0, {}, .name = _name }
#define BENCH_STAT_INIT(_name, BEGIN, APPLY, CLEAN)	      \
	(struct bench_stat) {                                 \
		.begin = LAT_STAT_INIT(BEGIN),                \
		.apply = LAT_STAT_INIT(APPLY),                \
		.clean = LAT_STAT_INIT(CLEAN),                \
		.total = LAT_STAT_INIT("Total"),              \
		.name = _name                                 \
	}
#define INIT_LAT_STAT(name, _name)                            \
	struct lat_stat name = LAT_STAT_INIT(_name);
// clang-format on

#endif /* _BENCH_COMMON_H_ */
