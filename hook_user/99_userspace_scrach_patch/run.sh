gcc -fpatchable-function-entry=8 -o test_program test_program.c
gcc -shared -o patch_function.so patch_function.c
gcc -shared -o ulp_lib.so ulp_lib.c -ldl

# run binary with LD_PRELOAD=ulp_lib.so
# LD_PRELOAD constructor will:
# 1. find original/patch tgt_func
# 2. change original tgt_func memory protection
# 3. patch tgt_func with JMP instruction by calculating offset
# 4. restore origin tgt_func memory protection
#
gcc -fpatchable-function-entry=8 -o test_program test_program.c
./test_program &

gcc -shared -o patch_function.so patch_function.c

gcc -shared -o ulp_lib.so ulp_lib.c -ldl

kill <test_program_pid> # 기존 프로세스 종료
LD_PRELOAD=./ulp_lib.so ./test_program &

gcc -o ulp_trigger ulp_trigger.c -ldl
./ulp_trigger <test_program_pid>
