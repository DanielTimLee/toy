# memmap

## AARCH64 Boot MEMBLOCK config

start_kernel()->setup_arch()

- setup_machine_fdt()
    - fixmap_remap_fdt() + memblock_reserve()
      (memblock->reserved = DT??)         (ex. 0x48600000/0x8b24)
    - early_init_dt_scan/nodes/memory()
      (memblock->memory   = RAM0)         (ex. 0x40000000/0x20000000)

- arm64_memblock_init()
    - if (phys_initrd_size)
      (memblock->reserved = INITRD)       (ex. 0x48000000/0x5b100)
    - memblock_reserve(__pa_symbol(_stext), _end - _stext);
      (memblock->reserved = KERNEL)       (ex. 0x40210000/0x2e30000)
    - early_init_fdt_scan_reserved_mem();


start_kernel()->setup_arch()->arm64_memblock_init->early_init_fdt_scan_reserved_mem();
- fdt_scan_reserved_mem()->__reserved_mem_reserve_reg->early_init_dt_reserve_memory()
    - if (!no_map)
      (memblock->memory   = RESERVE_MEM1) (ex. 0x40000000/0x1000)
      (memblock->reserved = RESERVE_MEM1) (ex. 0x40000000/0x1000)
    - else if (no_map)
      (memblock->memory   = RESERVE_MEM2) (ex. 0x40001000/0x1000)

    -> This makes
      (memblock->memory   = RAM0)         (ex. 0x40002000/0x1fffe000)
