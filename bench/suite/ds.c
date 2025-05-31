// suite/lock.c
//
#include <linux/slab.h>
#include <linux/ktime.h>
#include <linux/list.h>
#include <linux/xarray.h>
#include <linux/bitmap.h>
#include <linux/idr.h>
#include <linux/rhashtable.h>
#include <linux/maple_tree.h>
#include "../common.h"
#include "../suite.h"

extern struct data_entry *entries[MAX_ENTRY];
#define HLIST_BITS 10
#define HLIST_SIZE (1 << HLIST_BITS)

static LIST_HEAD(list);
static DEFINE_IDR(idr);
static DEFINE_XARRAY(xa);
static struct maple_tree mt;
static struct rhashtable ht;
static DECLARE_BITMAP(bitmap, MAX_ENTRY);
static struct hlist_head hlist_table[HLIST_SIZE];

static const struct rhashtable_params ht_params = {
	.head_offset = offsetof(struct data_entry, ht_node),
	.key_offset = offsetof(struct data_entry, key),
	.automatic_shrinking = false,
	.key_len = sizeof(u64),
};

static void list_insert(void *dummy)
{
	for (int i = 0; i < entry_cnt; i++)
		list_add_tail(&entries[i]->list, &list);
}

static void list_lookup(void *dummy)
{
	struct data_entry *e;
	for (int i = 0; i < entry_cnt; i++)
		list_for_each_entry(e, &list, list)
			if (e->key == i) {
				READ_ONCE(e->val);
				break;
			}
}

static void list_delete(void *dummy)
{
	for (int i = 0; i < entry_cnt; i++)
		list_del(&entries[i]->list);
}

/* HLIST_HEAD */
static u32 hlist_hash(u64 key) { return hash_64(key, HLIST_BITS); }

static void hlist_insert(void *dummy)
{
	for (int i = 0; i < entry_cnt; i++) {
		u32 hash = hlist_hash(entries[i]->key);
		hlist_add_head(&entries[i]->hlist, &hlist_table[hash]);
	}
}

static void hlist_lookup(void *dummy)
{
	struct data_entry *e;
	for (int i = 0; i < entry_cnt; i++) {
		u32 hash = hlist_hash(i);
		hlist_for_each_entry(e, &hlist_table[hash], hlist) {
			if (e->key == i) {
				READ_ONCE(e->val);
				break;
			}
		}
	}
}

static void hlist_delete(void *dummy)
{
	for (int i = 0; i < entry_cnt; i++)
		hlist_del(&entries[i]->hlist);
}

static void idr_insert(void *dummy)
{
	for (int i = 0; i < entry_cnt; i++)
		idr_alloc(&idr, entries[i], entries[i]->key,
			  entries[i]->key + 1, GFP_KERNEL);
}

static void idr_lookup(void *dummy)
{
	struct data_entry *e;

	for (int i = 0; i < entry_cnt; i++) {
		e = idr_find(&idr, i);
		READ_ONCE(e->val);
	}
}

static void idr_delete(void *dummy)
{
	for (int i = 0; i < entry_cnt; i++)
		idr_remove(&idr, i);
}

static void xarray_insert(void *dummy)
{
	for (int i = 0; i < entry_cnt; i++)
		xa_store(&xa, i, entries[i], GFP_KERNEL);
}

static void xarray_lookup(void *dummy)
{
	struct data_entry *e;

	for (int i = 0; i < entry_cnt; i++) {
		e = xa_load(&xa, i);
		READ_ONCE(e->val);
	}
}

static void xarray_delete(void *dummy)
{
	for (int i = 0; i < entry_cnt; i++)
		xa_erase(&xa, i);
}

static void mt_insert(void *dummy)
{
	for (int i = 0; i < entry_cnt; i++)
		mtree_store(&mt, entries[i]->key, entries[i], GFP_KERNEL);
}

static void mt_lookup(void *dummy)
{
	for (int i = 0; i < entry_cnt; i++)
		mtree_load(&mt, i);
}

static void mt_delete(void *dummy)
{
	for (int i = 0; i < entry_cnt; i++)
		mtree_erase(&mt, entries[i]->key);
}

static void rhash_insert(void *dummy)
{
	for (u64 i = 0; i < entry_cnt; i++)
		rhashtable_insert_fast(&ht, &entries[i]->ht_node, ht_params);
}

static void rhash_lookup(void *dummy)
{
	struct data_entry *e;

	// rhash use u64 for key
	for (u64 i = 0; i < entry_cnt; i++) {
		e = rhashtable_lookup_fast(&ht, &i, ht_params);
		READ_ONCE(e->val);
	}
}

static void rhash_delete(void *dummy)
{
	for (u64 i = 0; i < entry_cnt; i++)
		rhashtable_remove_fast(&ht, &entries[i]->ht_node, ht_params);
}

static void bitmap_insert(void *dummy)
{
	for (int i = 0; i < entry_cnt; i++)
		set_bit(entries[i]->bit_index, bitmap);
}

static void bitmap_lookup(void *dummy)
{
	for (int i = 0; i < entry_cnt; i++)
		if (test_bit(entries[i]->bit_index, bitmap))
			(void)entries[i]->val;
}

static void bitmap_delete(void *dummy)
{
	for (int i = 0; i < entry_cnt; i++)
		clear_bit(entries[i]->bit_index, bitmap);
}

static int init_entry(void)
{
	init_cache();
	init_entries();
	rhashtable_init(&ht, &ht_params);
	mt_init_flags(&mt, MT_FLAGS_ALLOC_RANGE);
	return 0;
}

static void cleanup_entry(void)
{
	rhashtable_destroy(&ht);
	mtree_destroy(&mt);
	cleanup_entries();
	cleanup_cache();
}

DS_UNIT(list, list_insert, list_lookup, list_delete);
DS_UNIT(hlist, hlist_insert, hlist_lookup, hlist_delete);
DS_UNIT(idr, idr_insert, idr_lookup, idr_delete);
DS_UNIT(xarray, xarray_insert, xarray_lookup, xarray_delete);
DS_UNIT(mtree, mt_insert, mt_lookup, mt_delete);
DS_UNIT(rhash, rhash_insert, rhash_lookup, rhash_delete);
DS_UNIT(bitmap, bitmap_insert, bitmap_lookup, bitmap_delete);

DS_TEST(list, init_entry, cleanup_entry);
DS_TEST(hlist, init_entry, cleanup_entry);
DS_TEST(idr, init_entry, cleanup_entry);
DS_TEST(xarray, init_entry, cleanup_entry);
DS_TEST(mtree, init_entry, cleanup_entry);
DS_TEST(rhash, init_entry, cleanup_entry);
DS_TEST(bitmap, init_entry, cleanup_entry);

struct bench_test *suite2_tests[] = { &list_test,   &hlist_test, &idr_test,
				      &xarray_test, &mtree_test, &rhash_test,
				      &bitmap_test };

struct bench_suite suite2_ds_tests = {
	.test_cnt = ARRAY_SIZE(suite2_tests),
	.tests = suite2_tests,
	.name = "ds",
};
