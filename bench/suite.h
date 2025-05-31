// suite.h
#ifndef _BENCH_SUITE_H_
#define _BENCH_SUITE_H_

#define LOCK_UNIT(NAME, RW, L, LOCK_FN, UNLOCK_FN, WORK_FN) \
	static struct bench_unit NAME##_##RW##_unit = {     \
		.priv = L,                                  \
		.begin = LOCK_FN,                           \
		.apply = WORK_FN,                           \
		.clean = UNLOCK_FN,                         \
		.type = BENCH_LOCK,                         \
	}

#define LOCK_RD(NAME, ...) LOCK_UNIT(NAME, reader, __VA_ARGS__)
#define LOCK_WR(NAME, ...) LOCK_UNIT(NAME, writer, __VA_ARGS__)

#define LOCK_TEST(NAME, INIT_FN, CLEANUP_FN)   \
	struct bench_test NAME##_test = {      \
		.name = #NAME,                 \
		.init = INIT_FN,               \
		.cleanup = CLEANUP_FN,         \
		.reader = &NAME##_reader_unit, \
		.writer = &NAME##_writer_unit, \
	}

#define DS_UNIT(NAME, INSERT_FN, LOOKUP_FN, DELETE_FN)  \
	static struct bench_unit NAME##_runner_unit = { \
		.begin = INSERT_FN,                     \
		.apply = LOOKUP_FN,                     \
		.clean = DELETE_FN,                     \
		.type = BENCH_DS,                       \
	}

#define DS_TEST(NAME, INIT_FN, CLEANUP_FN)     \
	struct bench_test NAME##_test = {      \
		.name = #NAME,                 \
		.init = INIT_FN,               \
		.cleanup = CLEANUP_FN,         \
		.runner = &NAME##_runner_unit, \
	}
#endif /* _BENCH_SUITE_H_ */
