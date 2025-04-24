#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <dlfcn.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <pid>\n", argv[0]);
        return 1;
    }

    pid_t pid = atoi(argv[1]);
    if (pid <= 0) {
        fprintf(stderr, "Invalid PID\n");
        return 1;
    }

    // 대상 프로세스에 시그널 전송 (예: 패치 트리거)
    // 실제로는 ptrace나 메모리 조작으로 패치 적용
    printf("Triggering patch for PID %d\n", pid);

    // libpulp의 ulp_trigger는 프로세스 메모리에 패치 로직을 주입
    // PoC에서는 단순히 라이브러리 함수 호출 가정
    void *handle = dlopen("./ulp_lib.so", RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "Failed to load ulp_lib.so: %s\n", dlerror());
        return 1;
    }

    void (*ulp_trigger)(void) = dlsym(handle, "ulp_trigger");
    if (!ulp_trigger) {
        fprintf(stderr, "Failed to find ulp_trigger: %s\n", dlerror());
        dlclose(handle);
        return 1;
    }

    ulp_trigger();
    dlclose(handle);

    printf("Patch triggered\n");
    return 0;
}
