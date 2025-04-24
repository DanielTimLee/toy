#include <stdio.h>
#include <unistd.h>

void tgt_func(void) {
	printf("tgt_func: waiting to be hooked!\n");
}

int main(void) {
	while (1) {
		tgt_func();
		sleep(1);
	}
	return 0;
}
