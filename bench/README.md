
### By waiting type

- SPIN: spinlock, bit_spinlock, rwlock, seqlock
- SLEEP: mutex
- NONE: cmpxchg, RCU/SRCU (writer should be serialized. spin)

### By use of lock

- RWLOCK: spinlock, bit_spinlock, mutex, rwlock
- WLOCK: seqlock
- NONE: cmpxchg, RCU/SRCU

### Comparison Statistics

- P95 shows the value below which 95% of the data points fall.
    - only 5% of the data points are slower than this value.
    - ex) P95: 64~ means most will fall under 64~128 ns.

- If `Avg ~= P95`: Reliable average
- If `Avg >> P95`: Trust P95 (High outliers skew the average)
- If `Avg << P95`: Most values are fast (but heavy outliers exist)

### Read Compare

== Suite: lock == (3000 ms, Reader / Writer: 4 / 0)

|           Name |        Ops/s |  Avg |  P95  | Work | Description                  |
|----------------|--------------|-----:|------:|-----:|------------------------------|
|        dummy_R |   10,071,487 |   99 |   64~ |  100 | No locking (baseline)        |
|      cmpxchg_R |    9,853,795 |  101 |   64~ |  101 | Atomic operations            |
|          seq_R |    8,578,332 |  101 |   64~ |  162 | Seqlock (read)               |
|     rcu_fast_R |    8,677,807 |  135 |  128~ |  115 | RCU fast path                |
|          rcu_R |    8,644,695 |  135 |  128~ |  116 | Standard RCU                 |
|    rcu_async_R |    8,532,220 |  137 |  128~ |  116 | Async call_rcu()             |
|   srcu_async_R |    7,673,933 |  164 |  128~ |  116 | SRCU async                   |
|    srcu_fast_R |    7,656,156 |  167 |  128~ |  117 | SRCU fast path               |
|         srcu_R |    7,558,153 |  170 |  128~ |  117 | Standard SRCU                |
|        rwsem_R |    5,931,094 |  253 |  256~ |  113 | down_read() based            |
|           rw_R |    5,291,826 |  322 |  256~ |  112 | Classic rwlock               |
|     spin_irq_R |    2,408,321 | 1154 |  256~ |   80 | Spinlock + IRQ save          |
|         spin_R |    2,400,018 | 1206 |  256~ |  111 | Standard spinlock            |
|     raw_spin_R |    2,335,812 | 1240 |  512~ |  110 | Raw spinlock                 |
| raw_spin_irq_R |    2,293,591 | 1227 | 2048~ |   91 | Raw spinlock + IRQ save      |
|     bit_spin_R |    1,723,577 | 1686 |  512~ |  229 | Bit-based atomic             |
|        mutex_R |    1,586,474 | 2007 | 2048~ |  109 | Standard mutex               |
|         bsem_R |       62,951 |57016 | 65536~|  103 | Blocking semaphore (slowest) |

- (Read) Lockless: cmpxchg, seq, rcu/srcu
- No Read Lock Contention: rwsem
- Spin Read Lock: rw, spin, bit_spin
- Sleep: mutex, bsem

### Write Compare

== Suite: lock == (3000 ms, Reader / Writer: 0 / 2)

|           Name |      Ops/s |  Avg |   P95  | Work | Description                  |
|----------------|------------|-----:|-------:|-----:|------------------------------|
|        dummy_W |  4,289,856 |   98 |   64~  |  172 | No locking (baseline)        |
|      cmpxchg_W |  4,234,531 |   98 |   64~  |  177 | Atomic operations            |
|     bit_spin_W |  2,491,741 |  390 |  512~  |  155 | Bit-based atomic             |
|    spin_irq_W  |  2,445,932 |  366 |  256~  |  151 | Spinlock + IRQ save          |
| raw_spin_irq_W |  2,389,578 |  380 |  256~  |  152 | Raw spinlock + IRQ save      |
|    rcu_async_W |  2,130,192 |   99 |   64~  |  642 | Async call_rcu()             |
|         spin_W |  2,102,260 |  481 |  512~  |  182 | Standard spinlock            |
|     raw_spin_W |  2,091,738 |  485 |  512~  |  182 | Raw spinlock                 |
|           rw_W |  2,067,805 |  497 |  512~  |  182 | Classic rwlock               |
|        rwsem_W |  2,029,033 |  506 |  512~  |  182 | down_write() based           |
|   srcu_async_W |  1,940,058 |  122 |  128~  |  664 | SRCU async                   |
|          seq_W |  1,755,860 |  651 |  512~  |  190 | Seqlock (write)              |
|        mutex_W |  1,577,091 |  674 |  512~  |  183 | Standard mutex               |
|    srcu_fast_W |     44,228 |  111 |  128~  |  492 | SRCU fast path               |
|     rcu_fast_W |     34,314 |  110 |   64~  |  348 | RCU fast path                |
|         bsem_W |     67,133 |23,625|16384~  |  174 | Blocking semaphore (slowest) |
|         srcu_W |      1,236 |  127 |  128~  |  613 | Standard SRCU                |
|          rcu_W |        162 |  142 |  256~  |  962 | Standard RCU                 |

- Lockless: dummy, cmpxchg
- Spin Write Lock: bit_spin, spin, rw, seq (but slower than rwsem)
- Sleepable Write Lock: rwsem, mutex, bsem
- RCU (includes spinlock to serialize write)
    - Asynchronous: fastest due to call_rcu/kfree_rcu
    - Fast variant: synchronize_rcu_expedited() reduces Grace Period
    - SRCU vs RCU : wait only one instance vs waits system global Grace Period

### Read/Write Compare

== Suite: lock == (3000 ms, Reader / Writer: 4 / 2)

| Name         | Read Ops/s | Write Ops/s | Avg R | P95 R | Work R | Avg W | P95 W | Work W | Description             |
|--------------|------------|-------------|-------|-------|--------|-------|-------|--------|-------------------------|
| dummy        | 7,755,167  | 11,314,480  | 100   |  64~  | 215    | 99    |  64~  | 241    | No locking (baseline)   |
| cmpxchg      | 7,940,915  | 11,045,364  | 101   |  64~  | 201    | 100   |  64~  | 254    | Atomic operations       |
| rcu_async    | 7,625,243  |  6,915,503  | 137   | 128~  | 174    | 102   |  64~  | 578    | Async call_rcu()        |
| srcu_async   | 6,079,571  |  4,902,683  | 197   | 256~  | 157    | 133   | 128~  | 847    | SRCU async              |
| seq          | 4,798,955  |  3,849,467  | 99    |  64~  | 533    | 927   | 1024~ | 264    | Seqlock (read/write)    |
| rcu          | 8,635,306  |        434  | 136   | 128~  | 115    | 152   | 128~  | 973    | Standard RCU            |
| rcu_fast     | 8,110,449  |     56,033  | 143   | 128~  | 124    | 109   |  64~  | 613    | RCU fast path           |
| srcu         | 7,024,262  |      2,499  | 178   | 128~  | 117    | 142   | 128~  | 741    | Standard SRCU           |
| srcu_fast    | 6,543,495  |     62,018  | 197   | 256~  | 126    | 113   | 128~  | 564    | SRCU fast path          |
| spin_irq     | 1,577,682  |  2,396,203  | 2045  | 1024~ | 84     | 1991  | 1024~ | 108    | Spinlock + IRQ save     |
| raw_spin_irq | 1,567,254  |  2,404,065  | 2061  | 1024~ | 84     | 1988  |  512~ | 109    | Raw spinlock + IRQ save |
| rw           | 1,422,323  |  2,297,539  | 2279  | 4096~ | 135    | 1996  | 2048~ | 184    | Classic rwlock          |
| spin         | 1,097,384  |  2,117,817  | 3074  | 1024~ | 111    | 2233  | 1024~ | 127    | Standard spinlock       |
| raw_spin     | 1,128,862  |  1,825,767  | 2935  | 2048~ | 111    | 2693  | 1024~ | 136    | Raw spinlock            |
| mutex        | 1,014,400  |  1,537,106  | 3395  | 2048~ | 150    | 3346  | 2048~ | 224    | Standard mutex          |
| bit_spin     |   647,407  |    225,356  | 4792  | 4096~ | 832    |25107  | 4096~ | 923    | Bit-based atomic        |
| rwsem        |   333,038  |    790,937  |11039  |32768~ | 128    | 5778  |32768~ | 139    | down_read/write()       |
| bsem         |    42,848  |     65,621  |86880  |65536~ | 147    |86731  |65536~ | 231    | Blocking semaphore      |

- **Ordered by total(R+W) Ops Rate**

- dummy, cmpxchg: Fastest. Lockless baseline.
- rcu_async: High throughput. Async write works well.
- rcu, srcu: Read is fast, but write is very slow (global wait).
- seq: Good read, but exclusive write slows down.
- spin, raw_spin: Balanced, but latency under contention.
- rw, rwsem: Moderate. rwsem sleeps. very high latency.
- mutex, bsem: Slowest. Sleep-based. not for fast paths.

- seqlock works well, but with many fields, it's harder to keep the data consistent.


### SPINLOCK variant

== Suite: lock == (3000 ms, Reader / Writer: 4 / 2)
[raw_spin_irq_R] ops=  4183184 dur= 3032ms rate=1379517/s
   Lock: min=  109 avg= 2394 p95= 1024~ p99= 4096~ ns
[raw_spin_irq_W] ops=  3052262 dur= 3032ms rate=1006565/s
   Lock: min=  110 avg= 1479 p95= 1024~ p99= 2048~ ns
[  raw_spin_R] ops=  3870608 dur= 3035ms rate=1275027/s
   Lock: min=  139 avg= 2560 p95= 1024~ p99= 4096~ ns
[  raw_spin_W] ops=  1759558 dur= 3035ms rate=579620/s
   Lock: min=  140 avg= 2866 p95= 2048~ p99= 4096~ ns
[  spin_irq_R] ops=  4838764 dur= 3032ms rate=1595418/s
   Lock: min=  110 avg= 2002 p95= 1024~ p99= 2048~ ns
[  spin_irq_W] ops=  2361732 dur= 3032ms rate=778700/s
   Lock: min=  110 avg= 2057 p95= 1024~ p99= 2048~ ns
[      spin_R] ops=  3694203 dur= 3034ms rate=1217315/s
   Lock: min=  140 avg= 2709 p95= 2048~ p99= 4096~ ns
[      spin_W] ops=  1939158 dur= 3034ms rate=638992/s

| Type          | Ops       | Rate     | Avg(ns) | Min(ns) |
|---------------|-----------|----------|---------|---------|
| raw_spin_irq  | 7,235,446 | 2,386,082| 1,936.5 | 109     |
| spin_irq      | 7,200,496 | 2,374,118| 2,029.5 | 110     |
| spin          | 5,633,361 | 1,856,307| 2,698.0 | 140     |
| raw_spin      | 5,630,166 | 1,854,647| 2,713.0 | 139     |

- spin and raw_spin performance are similar (only difference is lock ACCOUNTING)
- *_irq vairant improves perfmrnace


### Spinlock vs RWLOCK
== Suite: lock == (3000 ms, Reader / Writer: 4 / 0)
[  spinlock_R] ops=  6607199 dur= 3062ms rate=2157608/s
   Lock: min=  140 avg= 1350 p95= 1024~ p99= 2048~ ns
[    rwlock_R] ops= 17341794 dur= 3006ms rate=5767868/s
   Lock: min=  140 avg=  283 p95=  256~ p99=  512~ ns
[     mutex_R] ops=  4814955 dur= 3005ms rate=1602043/s
   Lock: min=  160 avg= 1991 p95= 2048~ p99= 2048~ ns

- RWLock outperforms at multiple readers.
    - Still RCU reader performs better, even with several writers

== Suite: lock == (3000 ms, Reader / Writer: 4 / 1)
[  spinlock_R] ops=  3376133 dur= 3033ms rate=1112956/s
   Lock: min=  140 avg= 3077 p95= 1024~ p99= 4096~ ns
[  spinlock_W] ops=  3075141 dur= 3033ms rate=1013733/s
   Lock: min=  140 avg=  471 p95=  512~ p99= 1024~ ns
[    rwlock_R] ops=  8532659 dur= 3006ms rate=2837996/s
   Lock: min=  140 avg=  870 p95= 1024~ p99= 2048~ ns
[    rwlock_W] ops=  2187730 dur= 3006ms rate=727647/s
   Lock: min=  140 avg=  736 p95=  512~ p99= 1024~ ns
[     mutex_R] ops=  4002959 dur= 3007ms rate=1331015/s
   Lock: min=  160 avg= 2503 p95= 2048~ p99= 2048~ ns
[     mutex_W] ops=  1005110 dur= 3007ms rate=334207/s
   Lock: min=  160 avg= 2501 p95= 2048~ p99= 2048~ ns

### Spinlock vs MUTEX
== Suite: lock == (3000 ms, Reader / Writer: 4 / 2)
[  spinlock_R] ops=  4026079 dur= 3043ms rate=1322705/s
   Lock: min=  140 avg= 2497 p95= 4096~ p99= 8192~ ns
[  spinlock_W] ops=  2203836 dur= 3043ms rate=724035/s
   Lock: min=  130 avg= 2248 p95= 4096~ p99= 8192~ ns
[    rwlock_R] ops=  2721764 dur= 3005ms rate=905644/s
   Lock: min=  140 avg= 3854 p95= 4096~ p99=16384~ ns
[    rwlock_W] ops=  2833094 dur= 3005ms rate=942688/s
   Lock: min=  140 avg= 1555 p95= 2048~ p99= 2048~ ns
[     mutex_R] ops=  3231778 dur= 3060ms rate=1055921/s
   Lock: min=  160 avg= 3278 p95= 2048~ p99= 4096~ ns
[     mutex_W] ops=  1621377 dur= 3060ms rate=529753/s
   Lock: min=  160 avg= 3196 p95= 2048~ p99= 4096~ ns

- spinlock is generally more faster, but fat-tail happens(p95, p99)
    - spinlock threads can wait much longer due to preemption, etc..
    - On the other hand, mutex(sleep) provides consistency with latency

- rwlock is slow when write ratio is high. (R:W = 4:2)
    - rwlock outperforms than spinlock (R:W = 4:1)

### cmpxchg
== Suite: lock == (3000 ms, Reader / Writer: 4 / 2)
[   cmpxchg_R] ops= 24108622 dur= 3062ms rate=7872915/s
   Lock: min=   90 avg=  100 p95=   64~ p99=  128~ ns
   Work: min=   90 avg=  206 p95=  128~ p99=  256~ ns
  Clean: min=   90 avg=  100 p95=   64~ p99=  128~ ns
[   cmpxchg_W] ops= 11174114 dur= 3062ms rate=3649020/s
   Lock: min=   90 avg=  101 p95=   64~ p99=  128~ ns
   Work: min=   90 avg=  245 p95=  256~ p99=  512~ ns
  Clean: min=   90 avg=  101 p95=   64~ p99=  128~ ns

- cmpxchg blazingly fast. Faster than RCU_async

### RCU

- Increase memory allocation, use kmem_cache
- Be aware of writer serialization. (Double free problem)

### RCU Compare
== Suite: lock == (3000 ms, Reader / Writer: 4 / 2)
[       rcu_R] ops= 26116091 dur= 3042ms rate=8583003/s
   Lock: min=  120 avg=  136 p95=  128~ p99=  128~ ns
   Work: min=  100 avg=  116 p95=   64~ p99=  128~ ns
  Clean: min=  100 avg=  112 p95=   64~ p99=  128~ ns
[       rcu_W] ops=      398 dur= 3042ms rate=130/s
   Lock: min=  100 avg=  184 p95=  128~ p99=  256~ ns
   Work: min=  490 avg= 1208 p95= 1024~ p99= 2048~ ns
  Clean: min=7974547 avg=15315994 p95=1048576~ p99=1048576~ ns
[  rcu_fast_R] ops= 23696020 dur= 3002ms rate=7893179/s
   Lock: min=  122 avg=  147 p95=  128~ p99=  128~ ns
   Work: min=  101 avg=  126 p95=  128~ p99=  128~ ns
  Clean: min=  101 avg=  124 p95=  128~ p99=  128~ ns
[  rcu_fast_W] ops=    51979 dur= 3002ms rate=17314/s
   Lock: min=   91 avg=  117 p95=  128~ p99=  128~ ns
   Work: min=  295 avg=  495 p95=  512~ p99=  512~ ns
  Clean: min=60224 avg=114767 p95=131072~ p99=131072~ ns
[ rcu_async_R] ops= 22755405 dur= 3052ms rate=7454850/s
   Lock: min=  131 avg=  140 p95=  128~ p99=  128~ ns
   Work: min=  101 avg=  177 p95=  128~ p99=  256~ ns
  Clean: min=  100 avg=  115 p95=  128~ p99=  128~ ns
[ rcu_async_W] ops=  6920233 dur= 3052ms rate=2267122/s
   Lock: min=   90 avg=  104 p95=   64~ p99=  128~ ns
   Work: min=  332 avg=  569 p95=  512~ p99= 1024~ ns
  Clean: min=   90 avg=  104 p95=   64~ p99=  128~ ns

[      srcu_R] ops= 20103019 dur= 3020ms rate=6656421/s
   Lock: min=  139 avg=  190 p95=  128~ p99=  256~ ns
   Work: min=   99 avg=  116 p95=   64~ p99=  128~ ns
  Clean: min=   99 avg=  135 p95=  128~ p99=  128~ ns
[      srcu_W] ops=     2019 dur= 3020ms rate=668/s
   Lock: min=  100 avg=  213 p95=  256~ p99=  256~ ns
   Work: min=  510 avg=  982 p95= 1024~ p99= 1024~ ns
  Clean: min=26983 avg=2995226 p95=1048576~ p99=1048576~ ns
[ srcu_fast_R] ops= 19391074 dur= 3029ms rate=6400428/s
   Lock: min=  139 avg=  200 p95=  256~ p99=  256~ ns
   Work: min=  100 avg=  132 p95=  128~ p99=  128~ ns
  Clean: min=   99 avg=  143 p95=  256~ p99=  256~ ns
[ srcu_fast_W] ops=    48608 dur= 3029ms rate=16044/s
   Lock: min=   89 avg=  159 p95=  128~ p99=  256~ ns
   Work: min=  330 avg=  720 p95= 1024~ p99= 1024~ ns
  Clean: min= 7805 avg=123536 p95=65536~ p99=1048576~ ns
[srcu_async_R] ops= 19381182 dur= 3044ms rate=6366815/s
   Lock: min=  139 avg=  181 p95=  256~ p99=  256~ ns
   Work: min=  109 avg=  157 p95=  128~ p99=  256~ ns
  Clean: min=   99 avg=  144 p95=  128~ p99=  256~ ns
[srcu_async_W] ops=  4362419 dur= 3044ms rate=1433076/s
   Lock: min=   89 avg=  160 p95=  128~ p99=  128~ ns
   Work: min=  330 avg=  914 p95= 1024~ p99= 1024~ ns
  Clean: min=   89 avg=  157 p95=  128~ p99=  128~ ns


- RCU read is blazingly fast. Increasing Write speed will decrease read performance
- [rcu_fast] synchronize_rcu_expedited() increase overall write throughput
    - due to increased write throughput, writers compete more at spinlock (work avg)
    - expedited() does not dramatically decrease read performance. (10% decrease)
- [rcu_async] has decent R/W ratio (30%, rcu: 0.001%, rcu_fast: 0.2%)
    - still like seqlock, 4R to 1W~2W looks decent

- RCU vs SRCU
    - rcu/srcu ops: ~25% drop
    - rcu_fast/srcu_ops: ~19% drop
    - rcu_async/srcu_async: ~14% drop
    - read lock latency: avg= ~40% slower, min=~15% slower

### LOCKLESS RCU WRITER (no serialize)
```
// Still has flaws. multiple writer can have same old, double free occurs
// MAYBE refcount for kfree'd entry might help?
static struct data_entry *do_rcu_write_lockless(struct data_entry *new)
{
	struct data_entry *old;
	// 1. Acquire Entry with Preempt / Reorder disable
	// 2. dump old data to new and modify
	// (if entry changed, retry again)
	do {
		rcu_read_lock();
		old = rcu_dereference(entry);
		rcu_read_unlock();

		new->key = old->key;
		new->val = old->val + 1;
	} while (rcu_access_pointer(entry) != old);

	// 3. Replace entry pointer
	rcu_assign_pointer(entry, new);

	return old;
}
```
