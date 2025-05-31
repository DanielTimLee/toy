// runner.c: bench test framework for kernel locks and data structures

#include <linux/compiler_attributes.h>
#include "common.h"

inline void collect_latency(struct lat_stat *stat, u64 lat)
{
	int idx = ilog2(lat) - ILOG_BASE;
	if (idx >= BUCKETS)
		idx = BUCKETS - 1;

	stat->tot += lat;
	stat->hist[(idx < 0) ? 0 : idx]++;
	stat->min = min_t(u64, stat->min, lat);
	stat->max = max_t(u64, stat->max, lat);
}

static void print_hist(u32 *hist)
{
	for (int i = 0; i < BUCKETS; i++) {
		u64 lo = 1ULL << (i + ILOG_BASE);
		u64 hi = 1ULL << (i + ILOG_BASE + 1);
		log_verbose("[%7llu ~ %7llu] : %u\n", lo, hi, hist[i]);
	}
}

static u64 __maybe_unused calc_count(u32 *hist)
{
	u64 cum = 0;
	for (int b = 0; b < BUCKETS; b++)
		cum += hist[b];

	return cum;
}

static u64 calc_p(int percentile, u32 *hist, u64 cnt)
{
	u64 cum = 0, target = (cnt * percentile) / 100;
	int b = 0;

	for (; b < BUCKETS; b++) {
		cum += hist[b];
		if (cum >= target)
			break;
	}

	return 1UL << (b + ILOG_BASE);
}

void print_latency(struct lat_stat *stat, u64 cnt)
{
	u64 p95 = calc_p(95, stat->hist, cnt);
	u64 p99 = calc_p(99, stat->hist, cnt);
	log_info("%7s: min=%5llu avg=%5llu p95=%5llu~ p99=%5llu~", stat->name,
		 stat->min, stat->tot / cnt, p95, p99);

	if (log_level >= LOG_DEBUG)
		pr_cont(" max=%llu", stat->max);

	pr_cont(" ns\n");
	print_hist(stat->hist);
}

struct bench_stat init_bench_stat(struct bench_unit *unit, char *_name)
{
	if (unit->type == BENCH_LOCK)
		return BENCH_STAT_INIT(_name, "Lock", "Work", "Clean");
	else if (unit->type == BENCH_DS)
		return BENCH_STAT_INIT(_name, "Insert", "Lookup", "Delete");
	else
		return BENCH_STAT_INIT(_name, "Lock", "Work", "Clean");
}

static void reduce_lat_stat(struct lat_stat *to, struct lat_stat *from)
{
	to->tot += from->tot;
	to->min = min_t(u64, to->min, from->min);
	to->max = max_t(u64, to->max, from->max);

	for (int i = 0; i < BUCKETS; i++)
		to->hist[i] += from->hist[i];
}

void reduce_bench_stat(struct bench_stat *to, struct bench_stat *from)
{
	reduce_lat_stat(&to->begin, &from->begin);
	reduce_lat_stat(&to->apply, &from->apply);
	reduce_lat_stat(&to->clean, &from->clean);
	reduce_lat_stat(&to->total, &from->total);
	to->op_cnt += from->op_cnt;
}

void report_stat(struct bench_stat *stat)
{
	u64 ns = ktime_to_ns(ktime_sub(stat->end, stat->start));
	u64 ops = stat->op_cnt;
	u64 ms = ns / 1000000;
	u64 rate = ns ? ops * NSEC_PER_SEC / ns : 0;

	if (!ops)
		return;

	log_info("[%12s] ops=%9llu dur=%5llums rate=%llu/s", stat->name, ops,
		 ms, rate);

	if (log_level >= LOG_DEBUG)
		pr_cont(" dur_ns=%llu", ns);

	pr_cont("\n");
	print_latency(&stat->begin, ops);
	print_latency(&stat->apply, ops);
	print_latency(&stat->clean, ops);
	print_latency(&stat->total, ops);
}
