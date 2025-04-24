#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>

#define NOP_SIZE 8
#define JMP_SIZE 5

// 대상 함수 주소 (수동으로 설정하거나 심볼 테이블에서 조회)
static void *target_func = NULL;
static void *patch_func = NULL;

// 초기화 함수: LD_PRELOAD 시 호출
__attribute__((constructor))
void init_ulp(void) {
    printf("ULP: Initializing...\n");

    // 대상 함수 주소 조회 (예: dlopen/dlsym으로 동적 조회 가능)
    // 여기서는 수동 설정 가정
    target_func = dlsym(RTLD_DEFAULT, "my_function");
    if (!target_func) {
        fprintf(stderr, "ULP: Failed to find my_function\n");
        return;
    }

    // 패치 함수 로드
    void *handle = dlopen("./patch_function.so", RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "ULP: Failed to load patch_function.so: %s\n", dlerror());
        return;
    }

    patch_func = dlsym(handle, "my_function");
    if (!patch_func) {
        fprintf(stderr, "ULP: Failed to find patched my_function\n");
        dlclose(handle);
        return;
    }

    printf("ULP: Target function at %p, patch function at %p\n", target_func, patch_func);
}

// 메모리 패칭: NOP를 JMP로 교체
int apply_patch(void) {
    if (!target_func || !patch_func) {
        fprintf(stderr, "ULP: Target or patch function not set\n");
        return -1;
    }

    // 메모리 보호 변경
    unsigned long page_size = sysconf(_SC_PAGESIZE);
    void *page_start = (void *)((unsigned long)target_func & ~(page_size - 1));
    if (mprotect(page_start, page_size, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        perror("ULP: mprotect failed");
        return -1;
    }

    // JMP 명령어 생성
    unsigned char jmp_code[JMP_SIZE] = {0xe9}; // JMP rel32
    *(uint32_t *)(jmp_code + 1) = (uint32_t)((unsigned long)patch_func - (unsigned long)target_func - JMP_SIZE);

    // NOP를 JMP로 교체
    memcpy(target_func, jmp_code, JMP_SIZE);

    // 메모리 보호 복원
    if (mprotect(page_start, page_size, PROT_READ | PROT_EXEC) != 0) {
        perror("ULP: mprotect restore failed");
        return -1;
    }

    printf("ULP: Patch applied\n");
    return 0;
}

// 외부에서 호출 가능한 패치 트리거 함수
void ulp_trigger(void) {
    apply_patch();
}
