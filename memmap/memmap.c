/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>

#define DEVICE_NAME "memmap"

static phys_addr_t phys_addr = 0;
static size_t phys_size = 0;
static int no_fault = 1;

module_param(phys_addr, ullong, 0444);
module_param(phys_size, ulong, 0444);
module_param(no_fault, int, 0444);

static int memmap_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long vsize = vma->vm_end - vma->vm_start;

	if (vsize > phys_size) {
		pr_err("requested mmap size too large\n");
		return -EINVAL;
	}

	return remap_pfn_range(vma, vma->vm_start, phys_addr >> PAGE_SHIFT,
			       vsize, vma->vm_page_prot);
}

static const struct file_operations memmap_fops = {
	.owner = THIS_MODULE,
	.mmap = memmap_mmap,
};

static struct miscdevice memmap_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DEVICE_NAME,
	.fops = &memmap_fops,
};

static int __init memmap_init(void)
{
	void *v = phys_to_virt(phys_addr);
	int ret;

	if (!phys_addr || !phys_size) {
		pr_err("phys_addr or phys_size not specified\n");
		return -EINVAL;
	}

	ret = misc_register(&memmap_miscdev);
	if (ret) {
		pr_err("failed to register misc device\n");
		return ret;
	}

	pr_err("/dev/%s registered. Starting mem access check\n", DEVICE_NAME);

	if (!pfn_valid(phys_addr >> PAGE_SHIFT)) {
		pr_err("PA 0x%llx is not backed by valid RAM\n",
		       (unsigned long long)phys_addr);
		pr_err("mmap() will not fail, but access will fault!\n");
		return 0;
	}

	if (no_fault) {
		ret = copy_from_kernel_nofault(&ret, v, sizeof(ret));
		if (ret) {
			pr_err("PA 0x%llx (VA %px) not mapped in MMU (no-map?)\n",
			       (unsigned long long)phys_addr, v);
			return 0;
		}
	} else {
		pr_err("PA 0x%llx (VA %px) writing. page fault if not mapped\n",
		       (unsigned long long)phys_addr, v);
		*(volatile uint32_t *)v = 0x12345678;
		ret = *(volatile uint32_t *)v;
	}

	pr_err("PA 0x%llx is mapped at VA %px, value = 0x%x\n",
	       (unsigned long long)phys_addr, v, ret);

	return 0;
}

static void __exit memmap_exit(void)
{
	misc_deregister(&memmap_miscdev);
	pr_info("module unloaded\n");
}

module_init(memmap_init);
module_exit(memmap_exit);

MODULE_LICENSE("GPL");
