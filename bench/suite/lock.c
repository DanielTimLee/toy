// suite/lock.c

#include <linux/compiler_attributes.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/rcutree.h>
#include <linux/kthread.h>
#include <linux/seqlock.h>
#include <linux/vmalloc.h>
#include <linux/bitops.h>
#include <linux/rwsem.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/srcu.h>
#include <linux/slab.h>
#include "../common.h"
#include "../suite.h"

// NOTE: bsem performance is worse than mutex. Whats the use of bsem?
// Unlike mutex, bsem doesn't have lock owner.
// = Other thread can unlock even not locked by itself
// e.g.) locked from thread / unlock from workqueue (or IRQ ctx)
static DEFINE_RAW_SPINLOCK(raw_spin);
static DEFINE_SPINLOCK(writer_lock); // only to serialize writers
static DEFINE_SEMAPHORE(bsem, 1);
static unsigned long bit_spin;
static DEFINE_SPINLOCK(spin);
static DECLARE_RWSEM(rwsem); // semaphore's writer is not exclusive
static DEFINE_SEQLOCK(seq);
static DEFINE_RWLOCK(rw);
static DEFINE_MUTEX(mtx);
DEFINE_STATIC_SRCU(srcu);

static struct data_entry __rcu *entry;
extern struct kmem_cache *entry_cache;

static int init_entry(void)
{
	init_cache();
	entry = kmem_cache_alloc(entry_cache, GFP_KERNEL);
	return 0;
}

static void cleanup_entry(void)
{
	// on rcu test, wait kfree_rcu(call_rcu)
	srcu_barrier(&srcu);
	rcu_barrier();
	kfree(entry);
	entry = NULL;
	cleanup_cache();
}

// basically, (void) entry->val / entry->val++;
static inline void do_read(void *L) { READ_ONCE(entry->val); }
static inline void do_write(void *L)
{
	WRITE_ONCE(entry->val, READ_ONCE(entry->val) + 1);
}

static inline void dummy_lock_fn(void *L) { /* no lock */ }
static inline void dummy_unlock_fn(void *L) { /* no lock */ }

// clang-format off
#define __PRIV ((struct bench_runner *)kthread_data(current))->priv
#define __FLAG ((struct bench_runner *)kthread_data(current))->flag

#define FN(name, stmt) static inline void name(void *L) { stmt; }
#define LOCK_FN(name, op, type)		FN(name, op((type *)L))
#define RCU_FN(name, stmt)		FN(name, stmt)

// SPIN wait locks
LOCK_FN(spin_lock_fn,              spin_lock,              spinlock_t);
LOCK_FN(spin_unlock_fn,            spin_unlock,            spinlock_t);
LOCK_FN(spin_irq_lock_fn,          spin_lock_irq,          spinlock_t);
LOCK_FN(spin_irq_unlock_fn,        spin_unlock_irq,        spinlock_t);
LOCK_FN(raw_spin_lock_fn,          raw_spin_lock,          raw_spinlock_t);
LOCK_FN(raw_spin_unlock_fn,        raw_spin_unlock,        raw_spinlock_t);
LOCK_FN(raw_spin_irq_lock_fn,      raw_spin_lock_irq,      raw_spinlock_t);
LOCK_FN(raw_spin_irq_unlock_fn,    raw_spin_unlock_irq,    raw_spinlock_t);

// Role of Memory Barrier: 1. Ordering / 2. Visibility (multicore write)
// Any Instruction can't go over (reorder over) barrier.
// Below are translated into instruction of:
//	smp_mb() = 'mfence' or 'lock' prefix
//	smp_rmb() = 'lfence' or 'lock' prefix
//	smp_wmb() = 'sfence' or 'lock' prefix
static inline void bit_spin_lock_fn(void *L)
{
	while (test_and_set_bit_lock(0, (unsigned long *)L))
		cpu_relax();
	// acquire barrier. Codes can't be run before lock
	smp_mb__after_atomic();
}

static inline void bit_spin_unlock_fn(void *L)
{
	// release barrier. includes smp_mb__before_atomic()
	clear_bit_unlock(0, (unsigned long *)L);
}

LOCK_FN(rw_read_lock_fn,           read_lock,              rwlock_t);
LOCK_FN(rw_read_unlock_fn,         read_unlock,            rwlock_t);
LOCK_FN(rw_write_lock_fn,          write_lock,             rwlock_t);
LOCK_FN(rw_write_unlock_fn,        write_unlock,           rwlock_t);

// SLEEP: mutex, binary semaphore, rw-semaphore
LOCK_FN(mutex_lock_fn,             mutex_lock,             struct mutex);
LOCK_FN(mutex_unlock_fn,           mutex_unlock,           struct mutex);
LOCK_FN(bsem_lock_fn,		   down,		   struct semaphore);
LOCK_FN(bsem_unlock_fn,		   up,			   struct semaphore);
LOCK_FN(rwsem_read_lock_fn,	   down_read,		   struct rw_semaphore);
LOCK_FN(rwsem_read_unlock_fn,	   up_read,		   struct rw_semaphore);
LOCK_FN(rwsem_write_lock_fn,	   down_write,		   struct rw_semaphore);
LOCK_FN(rwsem_write_unlock_fn,	   up_write,		   struct rw_semaphore);

// Others (Lockless): cmpxchg, seqlock, RCU/SRCU
LOCK_FN(cmpxchg_lock_fn,           dummy_lock_fn,          void);
LOCK_FN(cmpxchg_unlock_fn,         dummy_unlock_fn,        void);
LOCK_FN(seq_read_lock_fn,          dummy_lock_fn,          void);
LOCK_FN(seq_read_unlock_fn,        dummy_unlock_fn,        void);
LOCK_FN(seq_write_lock_fn,         write_seqlock,          seqlock_t);
LOCK_FN(seq_write_unlock_fn,       write_sequnlock,        seqlock_t);

#define SYNC_FREE(stmt) { stmt; kfree(__PRIV); }

RCU_FN(rcu_read_lock_fn,	   rcu_read_lock());
RCU_FN(rcu_read_unlock_fn,	   rcu_read_unlock());
RCU_FN(rcu_write_lock_fn,          dummy_lock_fn(NULL));
RCU_FN(rcu_write_unlock_fn,	   SYNC_FREE(synchronize_rcu()));
RCU_FN(rcu_fast_write_unlock_fn,   SYNC_FREE(synchronize_rcu_expedited()));
RCU_FN(rcu_async_write_unlock_fn,  dummy_unlock_fn(NULL));
// expedited() faster at least x50~ (n000 -> 100000)

RCU_FN(srcu_read_lock_fn,	   __FLAG = srcu_read_lock(&srcu));
RCU_FN(srcu_read_unlock_fn,	   srcu_read_unlock(&srcu, __FLAG));
RCU_FN(srcu_write_lock_fn,         dummy_lock_fn(NULL));
RCU_FN(srcu_write_unlock_fn,       SYNC_FREE(synchronize_srcu(&srcu)));
RCU_FN(srcu_fast_write_unlock_fn,  SYNC_FREE(synchronize_srcu_expedited(&srcu)));
RCU_FN(srcu_async_write_unlock_fn, dummy_unlock_fn(NULL));
// clang-format on

static void do_seq_read(void *L)
{
	u64 val;
	unsigned seq_retry;

	do {
		seq_retry = read_seqbegin(&seq); // includes smp_rmb()
		val = READ_ONCE(entry->val);
	} while (read_seqretry(&seq, seq_retry));

	(void)val;
}

static inline void do_cmpxchg_read(void *L)
{
	(void)smp_load_acquire(&entry->val);
}
static inline void do_cmpxchg_write(void *L)
{
	u64 old, new;

	do {
		// Make sure no other thread has changed. alternative (x86-64):
		/* smp_rmb(); old = READ_ONCE(entry->val); */
		old = smp_load_acquire(&entry->val);
		new = old + 1;
	} while (cmpxchg_release(&entry->val, old, new) != old); // atomic CAS
	// used cmpxchg release(smp_wmb) variant to block instruction reordering
	// due to release barrier, CPU store buffer/L1 caches eventually flushed
}

static void do_rcu_read(void *L)
{
	struct data_entry *e = rcu_dereference(entry);
	(void)e->val;
}

static void do_srcu_read(void *L)
{
	struct data_entry *e = srcu_dereference(entry, &srcu);
	(void)e->val;
}

// RCU/SRCU write must be serialized.
// If, multiple writer acquire same old, double free occurs
static struct data_entry *rcu_serialized_write(struct data_entry *new,
					       struct srcu_struct *srcu)
{
	struct data_entry *old;
	unsigned long flags;

	// 1. Acquire Entry with Preempt / Reorder disable
	spin_lock_irqsave(&writer_lock, flags);

	if (!srcu)
		old = rcu_dereference_protected(entry,
						lockdep_is_held(&writer_lock));
	else
		old = srcu_dereference_check(entry, srcu,
					     lockdep_is_held(&writer_lock));

	// 2. dump old data to new and modify
	new->key = old->key;
	new->val = old->val + 1;

	// 3. Replace entry pointer
	rcu_assign_pointer(entry, new);

	spin_unlock_irqrestore(&writer_lock, flags);
	return old;
}

static void rcu_kfree_cb(struct rcu_head *head)
{
	kfree(container_of(head, struct data_entry, rcu));
}

static inline void do_rcu_write_generic(bool async, bool SRCU)
{
	struct data_entry *new = kmem_cache_alloc(entry_cache, GFP_KERNEL);
	struct data_entry *old = rcu_serialized_write(new, SRCU ? &srcu : NULL);

	// sync : ctx->priv = old. Do kfree(old) at rcu_write_unlock_fn().
	// async: Register kfree(old) callback. eg) call_(s)rcu(&old->rcu, cb);
	if (!async)
		__PRIV = old;
	else {
		if (!SRCU)
			kfree_rcu(old, rcu);
		else
			call_srcu(&srcu, &old->rcu, rcu_kfree_cb);
	}
}

// clang-format off
static void do_rcu_write(void *L)        { do_rcu_write_generic(false, false); }
static void do_rcu_async_write(void *L)  { do_rcu_write_generic(true,  false); }
static void do_srcu_write(void *L)       { do_rcu_write_generic(false, true); }
static void do_srcu_async_write(void *L) { do_rcu_write_generic(true,  true); }

#define LOCK_ATTR(NAME, LOCK_PTR, R_LOCK_FN, R_UNLOCK_FN, R_FN, \
                                  W_LOCK_FN, W_UNLOCK_FN, W_FN) \
LOCK_RD(NAME, LOCK_PTR, R_LOCK_FN, R_UNLOCK_FN, R_FN);             \
LOCK_WR(NAME, LOCK_PTR, W_LOCK_FN, W_UNLOCK_FN, W_FN);             \
LOCK_TEST(NAME, init_entry, cleanup_entry)

#define LOCK_UNIFIED_FN(NAME, LOCK_PTR, R_FN, W_FN)                \
LOCK_ATTR(NAME, LOCK_PTR, NAME##_lock_fn, NAME##_unlock_fn, R_FN,  \
			  NAME##_lock_fn, NAME##_unlock_fn, W_FN)
#define LOCK_PARALLEL_FN(NAME, LOCK_PTR, R_FN, W_FN)               \
LOCK_ATTR(NAME, LOCK_PTR, NAME##_read_lock_fn, NAME##_read_unlock_fn, R_FN,\
	                  NAME##_write_lock_fn, NAME##_write_unlock_fn, W_FN)

#define LOCK_UNIFIED(NAME, LOCK_PTR)                               \
LOCK_UNIFIED_FN(NAME, LOCK_PTR, do_read, do_write)
#define LOCK_PARALLEL(NAME, LOCK_PTR)                              \
LOCK_PARALLEL_FN(NAME, LOCK_PTR, do_read, do_write)

#define RCU_TEST_FN(NAME, LOCK_PTR, R_FN, W_FN)                         \
LOCK_ATTR(NAME, LOCK_PTR, NAME##_read_lock_fn, NAME##_read_unlock_fn, R_FN, \
	                  NAME##_write_lock_fn, NAME##_write_unlock_fn, W_FN)

#define RCU_TEST_VARIANT_FN(NAME, VARIANT, LOCK_PTR, R_FN, W_FN)        \
LOCK_ATTR(NAME##_##VARIANT, LOCK_PTR,		                        \
                  NAME##_read_lock_fn, NAME##_read_unlock_fn, R_FN,     \
                  NAME##_write_lock_fn, NAME##_##VARIANT##_write_unlock_fn, W_FN)

// Only for reference max performance
// Spin: spin, raw_spin, bit_spin, rwlock
// (etc. ticket_spinlock)
LOCK_UNIFIED(dummy, NULL);
LOCK_UNIFIED(spin, &spin);
LOCK_UNIFIED(spin_irq, &spin);
LOCK_UNIFIED(raw_spin, &raw_spin);
LOCK_UNIFIED(raw_spin_irq, &raw_spin);
LOCK_UNIFIED(bit_spin, &bit_spin);

LOCK_PARALLEL(rw, &rw);

// Sleep: mutex, binary semaphore, rw-semaphore
// (etc. multiple lock scenario: wait-wound mutex)
LOCK_UNIFIED(mutex, &mtx);
LOCK_UNIFIED(bsem, &bsem);
LOCK_PARALLEL(rwsem, &rwsem);

// Others (Lockless): cmpxchg, seqlock, RCU/SRCU
LOCK_UNIFIED_FN(cmpxchg, NULL, do_cmpxchg_read, do_cmpxchg_write);
LOCK_PARALLEL_FN(seq, &seq, do_seq_read, do_write);

RCU_TEST_FN(rcu, NULL, do_rcu_read, do_rcu_write);
RCU_TEST_FN(srcu, NULL, do_srcu_read, do_srcu_write);
RCU_TEST_VARIANT_FN(rcu, fast, NULL, do_rcu_read, do_rcu_write);
RCU_TEST_VARIANT_FN(rcu, async, NULL, do_rcu_read, do_rcu_async_write);
RCU_TEST_VARIANT_FN(srcu, fast, NULL, do_srcu_read, do_srcu_write);
RCU_TEST_VARIANT_FN(srcu, async, NULL, do_srcu_read, do_srcu_async_write);

struct bench_test *suite1_tests[] = {
	&spin_test,     &spin_irq_test, &raw_spin_test, &raw_spin_irq_test,
	&bit_spin_test, &rw_test,
	&mutex_test,    &bsem_test,     &rwsem_test,	// Sleep based
	&cmpxchg_test,  &seq_test,      &rcu_test,      &rcu_fast_test, &rcu_async_test,
	&srcu_test,     &srcu_fast_test,&srcu_async_test,&dummy_test
};
// clang-format on

struct bench_suite suite1_lock_tests = {
	.test_cnt = ARRAY_SIZE(suite1_tests),
	.tests = suite1_tests,
	.name = "lock",
};
