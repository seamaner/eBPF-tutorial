load:
    bpf_check
        bpf_verifier_ops
        check_cfg                //深度优先
        do_check_main
            do_check_common
                do_check
        check_max_stack_depth
    sanitize_dead_code

    bpf_prog_select_runtime 
        bpf_prog_select_func

interpreters
```
static unsigned int (*interpreters[])(const void *ctx,
                      const struct bpf_insn *insn) = {
EVAL6(PROG_NAME_LIST, 32, 64, 96, 128, 160, 192)
EVAL6(PROG_NAME_LIST, 224, 256, 288, 320, 352, 384)
EVAL4(PROG_NAME_LIST, 416, 448, 480, 512)
};
```
gcc -nostdinc -E core.c展开后：
```
static unsigned int (*interpreters[])(const void *ctx,
          const struct bpf_insn *insn) = {
__bpf_prog_run32, __bpf_prog_run64, __bpf_prog_run96, __bpf_prog_run128, __bpf_prog_run160, __bpf_prog_run192,
__bpf_prog_run224, __bpf_prog_run256, __bpf_prog_run288, __bpf_prog_run320, __bpf_prog_run352, __bpf_prog_run384,
__bpf_prog_run416, __bpf_prog_run448, __bpf_prog_run480, __bpf_prog_run512,
};
```
__bpf_prog_runxx函数也是宏定义的：
```
1742 static unsigned int PROG_NAME(stack_size)(const void *ctx, const struct bpf_insn *insn) \
1743 { \
1744     u64 stack[stack_size / sizeof(u64)]; \
1745     u64 regs[MAX_BPF_EXT_REG]; \
1746 \
1747     FP = (u64) (unsigned long) &stack[ARRAY_SIZE(stack)]; \
1748     ARG1 = (u64) (unsigned long) ctx; \
1749     return ___bpf_prog_run(regs, insn); \
1750 }
```
FP、ARG1都是named register，FP是栈顶指针，
```
#define FP  regs[BPF_REG_FP]
#define ARG1    regs[BPF_REG_ARG1]
```
展开后：
```
static unsigned int __bpf_prog_run32(const void *ctx, const struct bpf_insn *insn) 
{ 
    u64 stack[32 / sizeof(u64)]; 
    u64 regs[MAX_BPF_EXT_REG]; 
    regs[BPF_REG_FP] = (u64) (unsigned long) &stack[ARRAY_SIZE(stack)]; 
    regs[BPF_REG_ARG1] = (     u64) (unsigned long) ctx; 
    return ___bpf_prog_run(regs, insn); 
}
```
