/*
 * pci_driver.c
 */

#include "linux/dma-mapping.h"
#include "linux/gfp_types.h"
#include "linux/types.h"
#include "linux/uaccess.h"
#include "linux/wait.h"
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
#define IO_DMA_SRC 0x80
#define IO_DMA_DST 0x88
#define IO_DMA_CNT 0x90
#define IO_DMA_CMD 0x98
#define IO_DMA_SET 0x100
#define BAR 0

// Edu Device Internal Buffer Size/Address/BUS WIDTH
#define DMA_SIZE 4096
#define DMA_BASE 0x40000
#define DMA_ADDR_WIDTH_BITS 28

// Start Flag. Cleared on DMA complete.
// device -> RAM. Otherwise: RAM -> dev.
#define DMA_CMD 0x1
#define DMA_FROM_DEV 0x2
#define DMA_IRQ 0x4

// Flag for notify DMA transfer complete
static volatile bool transfer_done = false;
static DECLARE_WAIT_QUEUE_HEAD(wait_queue);

static int use_coherent = 1;
module_param(use_coherent, int, 0644);
MODULE_PARM_DESC(use_coherent, "1=dma_alloc_coherent 0=manual DMA with cache problem");

static struct pci_device_id pci_ids[] = {
	{PCI_DEVICE(QEMU_VENDOR_ID, EDU_DEVICE_ID), },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, pci_ids);

static struct page *manual_page;
static struct pci_dev *pdev;
static void __iomem *mmio;
dma_addr_t dma_handle;
void *dma_page_addr;
static int major;

static irqreturn_t irq_handler(int irq, void *dev)
{
	u32 irq_status;
	if (*(int *)dev != major)
		return IRQ_NONE;

	irq_status = ioread32(mmio + IO_IRQ_STATUS);
	if (irq_status & IO_DMA_SET) {
		transfer_done = true;
		wake_up_interruptible(&wait_queue);
	}
	iowrite32(irq_status, mmio + IO_IRQ_ACK);
	return IRQ_HANDLED;
}

static ssize_t read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
	ssize_t ret;
	if (len != DMA_SIZE || *off != 0)
		return -EINVAL;

	iowrite32(DMA_BASE, mmio + IO_DMA_SRC);
	iowrite32((u32)dma_handle, mmio + IO_DMA_DST);
	iowrite32(DMA_SIZE, mmio + IO_DMA_CNT);
	iowrite32(DMA_CMD | DMA_FROM_DEV | DMA_IRQ, mmio + IO_DMA_CMD);

	// Sleep for a small amount of time to avoid busy-waiting
	// while (!(ioread32(mmio + IO_IRQ_STATUS) & 0x100)) {
	// 	msleep(10);
	// }

	if (wait_event_interruptible(wait_queue, transfer_done))
		return -EINTR;

	if (!use_coherent)
		dma_sync_single_for_cpu(&pdev->dev, dma_handle, DMA_SIZE, DMA_FROM_DEVICE);

	// returns failed bytes. On copy complete, returns 0
	ret = copy_to_user(buf, dma_page_addr, len);
	transfer_done = false;
	*off = 0;
	return ret ? -EFAULT : len;
}

static ssize_t write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
	ssize_t ret;
	if (len != DMA_SIZE || *off != 0)
		return -EINVAL;

	// returns failed bytes. On copy complete, returns 0
	ret = copy_from_user(dma_page_addr, buf, len);
	if (ret)
		return -EFAULT;

	if (!use_coherent)
		dma_sync_single_for_device(&pdev->dev, dma_handle, DMA_SIZE, DMA_TO_DEVICE);

	iowrite32((u32)dma_handle, mmio + IO_DMA_SRC);
	iowrite32(DMA_BASE, mmio + IO_DMA_DST);
	iowrite32(DMA_SIZE, mmio + IO_DMA_CNT);
	iowrite32(DMA_CMD | DMA_IRQ, mmio + IO_DMA_CMD);

	// Sleep for a small amount of time to avoid busy-waiting
	// while (!(ioread32(mmio + IO_IRQ_STATUS) & 0x100)) {
	// 	msleep(10);
	// }

	if (wait_event_interruptible(wait_queue, transfer_done))
		return -EINTR;

	transfer_done = false;
	*off = 0;
	return len - ret;
}

static struct file_operations fops = {
	.owner	= THIS_MODULE,
	.read	= read,
	.write	= write,
};

static void pci_remove(struct pci_dev *dev)
{
	pr_info("remove EDU pci kernel module\n");
	free_irq(pdev->irq, &major);

	if (!use_coherent) {
		dma_unmap_page(&pdev->dev, dma_handle, DMA_SIZE, DMA_BIDIRECTIONAL);
		__free_pages(manual_page, 0);
	} else {
		dma_free_coherent(&pdev->dev, DMA_SIZE, dma_page_addr, dma_handle);
	}

	pci_release_region(dev, BAR);
	unregister_chrdev(major, CDEV_NAME);
}

static int pci_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	dev_info(&dev->dev, "pci_probe\n");
	major = register_chrdev(0, CDEV_NAME, &fops);
	pdev = dev;

	if (pci_enable_device(dev) < 0)
		goto error;

	if (pci_request_region(dev, BAR, "EDU_BAR0"))
		goto error;

	mmio = pci_iomap(dev, BAR, pci_resource_len(dev, BAR));
	dev_info(&dev->dev, "BAR %d mapped to %px\n", BAR, mmio);

	if (request_irq(dev->irq, irq_handler, IRQF_SHARED, "edu_pci_irq0", &major) < 0)
		goto error;

	// Alloc new page for DMA
	dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(DMA_ADDR_WIDTH_BITS));

	if (use_coherent) {
		dma_page_addr = dma_alloc_coherent(&pdev->dev, DMA_SIZE, &dma_handle, GFP_ATOMIC);
	} else {
		manual_page = alloc_pages(GFP_KERNEL | GFP_DMA, 0);
		dma_page_addr = page_address(manual_page);
		dma_handle = dma_map_page(&pdev->dev, manual_page, 0, DMA_SIZE, DMA_BIDIRECTIONAL);
	}

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
