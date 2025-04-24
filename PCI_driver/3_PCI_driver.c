/*
 * pci_driver.c
 */

#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>

#define CDEV_NAME "edu_pci"
#define QEMU_VENDOR_ID 0x1234
#define EDU_DEVICE_ID 0x11e8
#define IO_IRQ_STATUS 0x24
#define IO_IRQ_ACK 0x64
#define BAR 0

static struct pci_device_id pci_ids[] = {
	{PCI_DEVICE(QEMU_VENDOR_ID, EDU_DEVICE_ID), },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, pci_ids);

static struct pci_dev *pdev;
static void __iomem *mmio;
static int major;

static irqreturn_t irq_handler(int irq, void *dev)
{
	irqreturn_t ret;
	u32 irq_status;
	int devi;

	devi = *(int *)dev;
	if (devi == major) {
		irq_status = ioread32(mmio + IO_IRQ_STATUS);
		pr_info("irq_handler irq = %d dev = %d irq_status = %llx\n",
			irq, devi, (unsigned long long)irq_status);

		// IRQ OFF
		iowrite32(irq_status, mmio + IO_IRQ_ACK);
		ret = IRQ_HANDLED;
	} else {
		ret = IRQ_NONE;
	}
	return ret;
}

static ssize_t read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
	ssize_t ret;
	u32 kbuf;

	if (*off % 4 || len == 0) {
		ret = 0;
	} else {
		kbuf = ioread32(mmio + *off);
		if (copy_to_user(buf, (void *)&kbuf, 4)) {
			ret = -EFAULT;
		} else {
			ret = 4;
			(*off)++;
		}
	}
	return ret;
}

static ssize_t write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
	ssize_t ret;
	u32 kbuf;

	ret = len;
	if (!(*off % 4)) {
		if (copy_from_user((void *)&kbuf, buf, 4) || len != 4) {
			ret = -EFAULT;
		} else {
			iowrite32(kbuf, mmio + *off);
		}
	}
	return ret;
}

static loff_t llseek(struct file *filp, loff_t off, int whence)
{
	filp->f_pos = off;
	return off;
}

static int mmap(struct file *file, struct vm_area_struct *vma)
{
	int status;

	// 1. Fetch BAR 0 IO(Physcial/BUS) address
	// 2. Divide by PAGE_SHIFT to get PFN (page number)
	// * BAR0 address is not real page, instead just physical address.
	vma->vm_pgoff = pci_resource_start(pdev, BAR) >> PAGE_SHIFT;

	// io_remap_pfn_range maps USER VA(vm_start) to Phy PFN(vm_pgoff)
	status = io_remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
				    vma->vm_end - vma->vm_start,
				    vma->vm_page_prot);
	if (status) {
		dev_err(&pdev->dev, "Error mapping IO device\n");
		return -status;
	}
	return 0;
}

static struct file_operations fops = {
	.owner	= THIS_MODULE,
	.llseek = llseek,
	.read	= read,
	.write	= write,
	.mmap	= mmap,
};

static void pci_remove(struct pci_dev *dev)
{
	pr_info("remove EDU pci kernel module\n");
	free_irq(pdev->irq, &major);
	pci_release_region(dev, BAR);
	unregister_chrdev(major, CDEV_NAME);
}

static int pci_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	// unlike pr_info, dev_info will print like below.
	//
	// <level><kernel-module> <pci-address>: <message>
	// ex. <6>edu_pci 0000:00:01.0: pci_probe
	dev_info(&dev->dev, "pci_probe\n");
	major = register_chrdev(0, CDEV_NAME, &fops);
	pdev = dev;

	if (pci_enable_device(dev) < 0)
		goto error;

	// Now, BAR already have BUS(physical) address but not claimed.
	// Address could be conflict with other devices unless request_region
	if (pci_request_region(dev, BAR, "EDU_BAR0"))
		goto error;

	// Configure for MMIO.
	// fetch virtual address for this region
	mmio = pci_iomap(dev, BAR, pci_resource_len(dev, BAR));
	dev_info(&dev->dev, "BAR %d mapped to %px\n", BAR, mmio);

	if (request_irq(dev->irq, irq_handler, IRQF_SHARED, "edu_pci_irq0", &major) < 0)
		goto error;

	return 0;
error:
	return 1;
}

static struct pci_driver pci_driver = {
	.name = "edu_pci",
	.id_table = pci_ids,
	.probe = pci_probe,
	.remove = pci_remove,
};

static int __init pci_init(void) {
	return pci_register_driver(&pci_driver);
}

static void __exit pci_exit(void) {
	pci_unregister_driver(&pci_driver);
}

module_init(pci_init);
module_exit(pci_exit);

MODULE_LICENSE("GPL");
