## 什么是Tail-call
"tail-call"（尾调用）是指一个函数在执行结束时，最后一步调用的是另一个函数，且该调用的返回值直接作为当前函数的返回值。简单来说，就是当前函数的最后一步是调用另一个函数，并将该函数的返回值返回给当前函数的调用者，而不需要在当前函数中进行其他的操作。

尾调用优化是一种编译器优化技术，它可以在某些情况下避免在函数调用时创建新的栈帧，从而减少函数调用的开销和内存消耗。这种优化通常在函数嵌套调用较深或需要进行递归计算时会带来明显的性能提升。
看一个C语言计算斐波那契数列的例子。
```
#include <stdint.h>

static uint64_t fib_tail(uint64_t n, uint64_t a, uint64_t b)
{
    if (n == 0)
        return a;
    if (n == 1)
        return b;

    return fib_tail(n - 1, b, a + b);
}

uint64_t fib(uint64_t n)
{
    return fib_tail(n, 1, 1);
}
int main()
{
    int n = fib(10000000);
    printf("fib(%d) is %d\n", 10000000, n);
}
```
使用`gcc -c fib.c -o fib.o`编译，查看汇编代码（使用`objdump -d fib.o`），可以看到fib函数调用fib_tail(`call 0 <fib_tail>`):

```
0000000000000000 <fib_tail>:
   0:   f3 0f 1e fa             endbr64
   4:   55                      push   %rbp
   5:   48 89 e5                mov    %rsp,%rbp
   8:   48 83 ec 20             sub    $0x20,%rsp
   c:   48 89 7d f8             mov    %rdi,-0x8(%rbp)
  10:   48 89 75 f0             mov    %rsi,-0x10(%rbp)
  14:   48 89 55 e8             mov    %rdx,-0x18(%rbp)
  18:   48 83 7d f8 00          cmpq   $0x0,-0x8(%rbp)
  1d:   75 06                   jne    25 <fib_tail+0x25>
  1f:   48 8b 45 f0             mov    -0x10(%rbp),%rax
  23:   eb 2f                   jmp    54 <fib_tail+0x54>
  25:   48 83 7d f8 01          cmpq   $0x1,-0x8(%rbp)
  2a:   75 06                   jne    32 <fib_tail+0x32>
  2c:   48 8b 45 e8             mov    -0x18(%rbp),%rax
  30:   eb 22                   jmp    54 <fib_tail+0x54>
  32:   48 8b 55 f0             mov    -0x10(%rbp),%rdx
  36:   48 8b 45 e8             mov    -0x18(%rbp),%rax
  3a:   48 01 c2                add    %rax,%rdx
  3d:   48 8b 45 f8             mov    -0x8(%rbp),%rax
  41:   48 8d 48 ff             lea    -0x1(%rax),%rcx
  45:   48 8b 45 e8             mov    -0x18(%rbp),%rax
  49:   48 89 c6                mov    %rax,%rsi
  4c:   48 89 cf                mov    %rcx,%rdi
  4f:   e8 ac ff ff ff          call   0 <fib_tail>
  54:   c9                      leave
  55:   c3                      ret
```
`gcc fib.c -o fib`编译，为了便于查看执行结果，用gdb运行：
```
gdb ./fib
(gdb) run
Starting program: /home/ubuntu/ebpf/eBPF-tutorial/tail-call/fib
[Thread debugging using libthread_db enabled]
Using host libthread_db library "/lib/x86_64-linux-gnu/libthread_db.so.1".
Program received signal SIGSEGV, Segmentation fault.
0x0000555555555198 in fib_tail ()
(gdb) bt -1
#92469 0x000055555555519d in fib_tail ()
```
可以看到程序执行到92469次，栈溢出sement fault退出。

函数的最后一行是函数调用，由于调用返回后会接着返回，因此当前函数栈frame不需要保存，call可以优化成jmp。  
call相当于，push ret-address + jump ， 优化后省掉了压栈操作。这样Tail-call达到了栈不增长的效果。
gcc -O3优化后，可以看到call被优化掉：
```
fib.o:     file format elf64-x86-64


Disassembly of section .text:

0000000000000000 <fib>:
   0:   f3 0f 1e fa             endbr64
   4:   48 83 ff 01             cmp    $0x1,%rdi
   8:   76 2e                   jbe    38 <fib+0x38>
   a:   b8 01 00 00 00          mov    $0x1,%eax
   f:   ba 01 00 00 00          mov    $0x1,%edx
  14:   eb 0d                   jmp    23 <fib+0x23>
  16:   66 2e 0f 1f 84 00 00    cs nopw 0x0(%rax,%rax,1)
  1d:   00 00 00
  20:   4c 89 c0                mov    %r8,%rax
  23:   48 83 ef 01             sub    $0x1,%rdi
  27:   4c 8d 04 02             lea    (%rdx,%rax,1),%r8
  2b:   48 89 c2                mov    %rax,%rdx
  2e:   48 83 ff 01             cmp    $0x1,%rdi
  32:   75 ec                   jne    20 <fib+0x20>
  34:   4c 89 c0                mov    %r8,%rax
  37:   c3                      ret
  38:   41 b8 01 00 00 00       mov    $0x1,%r8d
  3e:   eb f4                   jmp    34 <fib+0x34>
```
优化后的fib可以正常运行，计算出结果：
```
gcc fib.c -O3 -o fib.o3
 ./fib.o3
fib(10000000) is 8644293272739028509
```
## 什么是eBPF Tail-call
```
       long bpf_tail_call(void *ctx, struct bpf_map *prog_array_map, u32
       index)

              Description
                     This special helper is used to trigger a "tail
                     call", or in other words, to jump into another eBPF
                     program. The same stack frame is used (but values
                     on stack and in registers for the caller are not
                     accessible to the callee). This mechanism allows
                     for program chaining, either for raising the
                     maximum number of available eBPF instructions, or
                     to execute given programs in conditional blocks.
                     For security reasons, there is an upper limit to
                     the number of successive tail calls that can be
                     performed.

                     Upon call of this helper, the program attempts to
                     jump into a program referenced at index index in
                     prog_array_map, a special map of type
                     BPF_MAP_TYPE_PROG_ARRAY, and passes ctx, a pointer
                     to the context.

                     If the call succeeds, the kernel immediately runs
                     the first instruction of the new program. This is
                     not a function call, and it never returns to the
                     previous program. If the call fails, then the
                     helper has no effect, and the caller continues to
                     run its subsequent instructions. A call can fail if
                     the destination program for the jump does not exist
                     (i.e. index is superior to the number of entries in
                     prog_array_map), or if the maximum number of tail
                     calls has been reached for this chain of programs.
                     This limit is defined in the kernel by the macro
                     MAX_TAIL_CALL_CNT (not accessible to user space),
                     which is currently set to 33.

              Return 0 on success, or a negative error in case of
                     failure.
```
