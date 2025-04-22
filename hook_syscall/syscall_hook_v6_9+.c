/*
  * syscall.c from sysprog21/LKMPG
  * only for v6.9+. livepatch solution
  *
  * - Saves the original sys_openat
  * - do livepatch.
  *
  *   * sys_openat happens very intensive.
  *   * might happen between livepatch and func ptr initalization.
  *   * In that case, use kprobe to initialize func ptr priorly.
  */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/livepatch.h>
#include <linux/kprobes.h>

long (*o_sys_openat)(const struct pt_regs *);

static long our_sys_openat(const struct pt_regs *regs)
{
	pr_info("openat hook. Warning: Nested call.\n");
	return o_sys_openat(regs);
}

// Note: some prefix functions are equal to do_sys*
// root@DK:~# cat /proc/kallsyms | grep sys_getpid
// ffffffff812a96d0 t __pfx___do_sys_getpid
// ffffffff812a96d0 T __pfx___x64_sys_getpid
// ffffffff812a96e0 t __do_sys_getpid
// ffffffff812a96e0 T __x64_sys_getpid
//
// __x64_sys_getpid and __do_sys_getpid are same symbol. (alias?)
//
// Must be global. If declared within __init, will be destroyed.
static struct klp_func funcs[] = {
	{ .old_name = "__x64_sys_openat", .new_func = our_sys_openat }, {}};
static struct klp_object objs[] = { { .funcs = funcs }, {} };
static struct klp_patch patch = { .mod = THIS_MODULE, .objs = objs };

static int __init patch_start(void)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 9, 0))
	pr_err("Only for v6.9+. Exiting.\n");
#else
	// use kprobe if livepatch's .old_func doesn't work properly.
	// * sys_openat can be intense, even between livepatch and ptr config
	/* struct kprobe kp = {.symbol_name = "__x64_sys_openat"}; */
	/* if (register_kprobe(&kp) < 0) */
	/* 	return 0; */

	/* o_sys_openat = (long (*)(const struct pt_regs *))kp.addr; */
	/* unregister_kprobe(&kp); */

	klp_enable_patch(&patch);

	o_sys_openat = funcs[0].old_func;
	pr_info("o_sys_openat %px\n", o_sys_openat);

#endif /* >= v6.9 */
	return 0;
}

static void __exit patch_end(void)
{
}

module_init(patch_start);
module_exit(patch_end);

MODULE_LICENSE("GPL");
MODULE_INFO(livepatch, "Y");
