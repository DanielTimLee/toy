#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define DEVICE_PATH "/dev/memmap"
#define MAP_SIZE 0x1000 // 4KB

int main(void)
{
	void *map;
	int fd = open(DEVICE_PATH, O_RDWR);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	map = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (map == MAP_FAILED) {
		perror("mmap");
		close(fd);
		return 1;
	}

	printf("Mapped %s at virtual address %p\n", DEVICE_PATH, map);

	uint32_t *p = (uint32_t *)map;
	*p = 0xDEADBEEF;
	printf("Wrote 0x%x at offset 0\n", *p);

	uint32_t val = *(uint32_t *)map;
	printf("Read  0x%x from offset 0\n", val);

	munmap(map, MAP_SIZE);
	close(fd);

	return 0;
}
