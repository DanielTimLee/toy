# Syscall Hook

From Linux v6.9+, system call table hooking does not work anymore due
to change of syscall mechanism. In short, it doesn't use array based
jump anymore. Instead it uses switch statement regarding speculative
execution.

https://stackoverflow.com/questions/78599971/hooking-syscall-by-modifying-sys-call-table-does-not-work
