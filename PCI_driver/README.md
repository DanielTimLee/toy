# PCI device

## How Linux Device Communicates?

- PIO/MMIO

```
  +----------------+         +----------------+         +----------------+
  | User/Kernel    |         | CPU/MMU        |         | Mem Controller |
  | Code           |         |                |         |                |
  |  VA Access [1] |-------->|  VA -> PA [2]  |-------->| Route PA [3]   |
  |                |         |                |         |                |
  +----------------+         +----------------+         +----------------+
                            (Return data     ^               |
                             to CPU/User) [7]+-----------+   | (MMIO to PCIe)
                                                         |   v
  +----------------+         +----------------+         +----------------+
  | PCIe Device    |         | PCIe Switch/   |         | PCIe Root      |
  | (Endpoint)     |         | Bridge         |         | Complex        |
  | Process TLP [6]|<--------| Route TLP [5]  |<--------| Create TLP [4] |
  |                |         |                |         |                |
  +----------------+         +----------------+         +----------------+
            |                                             ^
            | (Response)                                  |
            +---------------------------------------------+
```

- DMA

```
            +----------------------+
            |  Read Afterwards[6]  v
  +----------------+         +----------------+         +----------------+
  | Kernel Code    |         | CPU/MMU        |         | Mem Controller |
  | -Alloc Page &  |         |                |         |                |
  | Send DMA CMD[1]|         |  VA -> PA [7]  |-------->| Read  MEM [8]  |
  | With Page Addr |         |                |         | Write MEM [5]  |
  +----------------+         +----------------+         +----------------+
                |                                            ^
                +----------------------------------------+   | (DMA to MEM)
                  (MMIO to PCIe)                         v   |
  +----------------+         +----------------+         +----------------+
  | PCIe Device    |         | PCIe Switch/   |         | PCIe Root      |
  | (DMA Engine)   |         | Bridge         |         | Complex        |
  |                |<--------|                |<--------|                |
  | Create TLP [2] |-------->| Route TLP [3]  |-------->| Process TLP [4]|
  | (write to RAM) |         |                |         |                |
  +----------------+         +----------------+         +----------------+
```

### Does all of the devices support?

| Device Type   | PIO  | MMIO | DMA | Note                         |
|---------------|------|------|-----|------------------------------|
| GPIO          |  o   |  o   |  x  | legacy use PIO               |
| UART          |  o   |  o   |  ^  | some uses DMA                |
| SPI           |  o   |  o   |  o  | High-speed uses DMA          |
| I2C           |  o   |  o   |  o  | some uses MDA                |
| PCI/PCIe      |  o   |  o   |  o  | mostly MMIO+DMA              |
| SATA/IDE      |  o   |  o   |  o  | IDE(PIO) SATA(DMA)           |
| NVMe/SSD      |  x   |  o   |  o  | MMIO/large-scale DMA         |
| GPU           |  x   |  o   |  o  | mostly MMIO/DMA              |
| Network (NIC) |  x   |  o   |  o  | mostly DMA (high-speed)      |
| USB           |  x   |  o   |  o  | mostly DMA                   |
| Sound (ALSA)  |  x   |  o   |  o  | mostly DMA (prevent glitch)  |
| Watchdog      |  o   |  o   |  x  | simple timer control, (MMIO) |
| Timer/RTC     |  o   |  o   |  x  | Basic time control, (MMIO)   |
| SD/MMC        |  o   |  o   |  o  | mostly DMA                   |
| CAN Bus       |  o   |  o   |  o  | some uses DMA                |


### Linux Device Codes?

- PIO (old method)

```
// pci_resource_flags(dev, BAR0) should be IORESOURCE_IO
ulong pci_request_region(dev, BAR0, "EDU_PIO_BAR0");
phy_addr = pci_resource_start(dev, BAR);

u8 value = inb(phy_addr);         // PIO read
outb(value | 0x1, phy_addr);      // PIO write
```

- MMIO

```
// pci_resource_flags(dev, BAR) should be IORESOURCE_MEM
pci_request_region(dev, BAR, "EDU_MMIO_BAR0");
void __iomem *virt_addr = pci_iomap(dev, 0, pci_resource_len(dev, 0));

u8 value = ioread32(mmio_base);   // MMIO read
iowrite32(0x12345678, mmio_base); // MMIO write
```

- DMA Manual Approach (page alloc + cache handling)

```
struct page *pages = alloc_pages(GFP_KERNEL | GFP_DMA, order);
// Specify direction to handle cache correctly. (dma_alloc_coherent() handles by itself)
dma_addr_t dma_handle = dma_map_page(dev, pages, 0, PAGE_SIZE << order, DMA_BIDIRECTIONAL);
void *vaddr = page_address(pages);
size_t size = PAGE_SIZE << order;

// After write, call below to write cache directly to device
// if using dma_alloc_coherent(), no need for below cache handling
memset(vaddr, 0xAA, size);
dma_sync_single_for_device(dev, dma_handle, size, DMA_TO_DEVICE);
// send RAM->device DMA command

// Before read, call below to invalidate cache and read latest
// if using dma_alloc_coherent(), no need for below cache handling
// send device->RAM DMA command
dma_sync_single_for_cpu(dev, dma_handle, size, DMA_FROM_DEVICE);
data = (u32 *)vaddr;

// after done, unmap DMA mapping
dma_unmap_page(dev, dma_handle, PAGE_SIZE << order, DMA_BIDIRECTIONAL);
__free_pages(pages, order);
```

- DMA alloc coherent (auto page alloc / cache handling)

```
dma_addr_t dma_handle;
void *vaddr = dma_alloc_coherent(&(dev->dev), 4, &dma_handle, GFP_ATOMIC);

/* RAM -> device DMA request. */
// memset(vaddr, 0xAA, size);
*((volatile u32*)vaddr) = 0x12345678;
iowrite32((u32)dma_handle,      mmio_base + IO_DMA_SRC); // RAM address
iowrite32(DMA_BASE,             mmio_base + IO_DMA_DST); // Internal Buf addr
iowrite32(SIZE,                 mmio_base + IO_DMA_CNT);
iowrite32(DMA_CMD | DMA_IRQ,    mmio_base + IO_DMA_CMD);

/* device -> RAM DMA request. */
iowrite32(DMA_BASE,             mmio_base + IO_DMA_SRC); // Internal Buf addr
iowrite32((u32)dma_handle,      mmio_base + IO_DMA_DST); // RAM address
iowrite32(SIZE,                 mmio_base + IO_DMA_CNT);
iowrite32(DMA_CMD | DMA_IRQ,    mmio_base + IO_DMA_CMD);
*((volatile u32*)vaddr);

dma_free_coherent(dev, size, vaddr, dma_handle);
```

## How PCI BAR works?

Check 0_PCI_BAR.sh for details.

```
# dmesg -w
# [step 0] pci 0000:00:05.0: [1234:11e8] type 00 class 0x00ff00 conventional PCI endpoint
# [step 1] pci 0000:00:05.0: BAR 0 [mem 0x00000000-0x000fffff]
# [step 2] pci 0000:00:05.0: BAR 0 [mem 0x20000000-0x200fffff]: assigned
```

- Step 0. PCI subsystem identifies the device
- Step 1. Set BAR0 (location 10h) to ffffffff
    - PCI will return size like fff00000 (1MB)
- Step 2. PCI subsystem will choose physical address to map
    - PCI used 0x20000000 and set 'Memory Space enable' (register 04h=2)

- Now, PCI device realize where it's BAR is mapped to kernel memory space (physical addr)

## PCI driver with QEMU edu device

Implement PCI driver with QEMU --device edu
Or test with UIO PCI driver.

### EDU PCI device

Simple device, only calculates factorial.

- Using MMIO or DMA
- Notify by MSI or IRQ (hardwire)

#### How to?

- expose MMIO region with device node (e.g. mknod or module)
- write commands to MMIO region, and check results.
    - command: 0x00       / result: 0x010000edu
    - command: 0x04 + NUM / result: ~NUM (inverted value)
    - command: 0x08 + NUM / result: NUM!
    - command: 0x20       / result: status register (EduState->status)
    - command: 0x24       / result: interrupt status register (EduState->irq_status)
    - refer to 'edu.txt'


## PCI driver with QEMU ivshmem device

A scatter-gatther.

## DMABUF
