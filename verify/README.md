## load:

verify发生在load到内核的过程中，忽略掉load权限等基本检查、内存copy、内存分配等，bpf load操作主要有这么几步：
- 检查每一个eBPF指令，即verify
- 选择合适的解释函数，即interpreters
- 分配prog id
- 分配fd，并返回给bpf系统调用

```
bpf_prog_load
    bpf_check
    bpf_prog_select_runtime 
        bpf_prog_select_func  //从interpreters中找到fp->bpf_func = interpreters[(round_up(stack_depth, 32) / 32) - 1];
    bpf_prog_alloc_id
    bpf_prog_new_fd
```

## verify

//函数总共230行左右，首先为每一个指令insn分配一个struct bpf_insn_aux_data结构
- 分配struct bpf_verifier_env
- 分配bpf_insn_aux_data
- 检查是否有不能结束的loop
- 检查每一个bpf函数的指令
- 检查每一个指令的寄存器使用是否合法、内存使用是否合法
- 检查stack_depth,栈使用的大小，是否越界
- 检查是否有执行不到的分支、指令、无效指令

bpf_check
        //函数总共230行左右，首先为每一个指令insn分配一个struct bpf_insn_aux_data结构
        bpf_verifier_ops
        check_cfg                //深度优先, loop
        do_check_subprogs
        do_check_main
            do_check_common
                do_check        //检查每一个指令，opcode
        check_max_stack_depth
        sanitize_dead_code

```
struct bpf_insn_aux_data {
    union {
        enum bpf_reg_type ptr_type; /* pointer type for load/store insns */
        unsigned long map_ptr_state;    /* pointer/poison value for maps */
        s32 call_imm;           /* saved imm field of call insn */
        u32 alu_limit;          /* limit for add/sub register with pointer */
        struct {
            u32 map_index;      /* index into used_maps[] */
            u32 map_off;        /* offset from value base address */
        };
        struct {
            enum bpf_reg_type reg_type; /* type of pseudo_btf_id */
            union {
                struct {
                    struct btf *btf;
                    u32 btf_id; /* btf_id for struct typed var */
                };
                u32 mem_size;   /* mem_size for non-struct typed var */
            };
        } btf_var;
    };
    u64 map_key_state; /* constant (32 bit) key tracking for maps */
    int ctx_field_size; /* the ctx field size for load insn, maybe 0 */
    u32 seen; /* this insn was processed by the verifier at env->pass_cnt */
    bool sanitize_stack_spill; /* subject to Spectre v4 sanitation */
    bool zext_dst; /* this insn zero extends dst reg */
    u8 alu_state; /* used in combination with alu_limit */

    /* below fields are initialized once */
    unsigned int orig_idx; /* original instruction index */
    bool prune_point;
};
```

### function call check
BPF虚拟机使得可以在kernel环境里运行代码，如果BPF不能和内核交互，作用会是非常有限的。BPF函数调用目前支持3种(5.15)：
- check_func_call
  eBPF-to-eBPF call
- check_kfunc_call
  eBPF-to-kernel function call(5.13)
- check_helper_call
  eBPF to helper functions

## interpreters

interpreters函数最后都是调的__bpf_prog_run,差别只在于stack大小。
从这里也可以看出，stack最大是512.

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
__bpf_prog_runxx函数也是宏定义的,几个函数区别只是stack的大小, 最终都是调用__bpf_prog_run:
```
1740 #define PROG_NAME(stack_size) __bpf_prog_run##stack_size
1741 #define DEFINE_BPF_PROG_RUN(stack_size) \
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
FP、ARG1都是named register，FP是栈顶指针，初始化为栈顶stack尾部，因此FP也是往下增长的。
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
函数___bpf_prog_run在上述给定的context(stack, regs)下执行所有insn指令,
```
#define CONT     ({ insn++; goto select_insn; })
#define CONT_JMP ({ insn++; goto select_insn; })

select_insn:
    goto *jumptable[insn->code];
```
`jumptable`用GNU扩展[&&](https://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html)定义了每个指令的跳转位置，比如，`[BPF_JMP | BPF_TAIL_CALL] = &&JMP_TAIL_CALL,`。
每条指令执行完都调用`CONT`，返回到label select_insn处，接着跳转到下一条指令的执行代码，直至程序退出或执行或所有指令。
`___bpf_prog_run`使用`goto`代替`while - switch`实现循环，据说是用`goto`可以提高15%~20%的性能。

### JIT

在`bpf_prog_select_runtime`中,如果支持JIT会执行`bpf_int_jit_compile`函数，`kernel/bpf/core.c`中定义了`bpf_int_jit_compile`是一个weak函数:
```
2390 struct bpf_prog * __weak bpf_int_jit_compile(struct bpf_prog *prog)
2391 {
2392     return prog;
2393 }
```

看得出这个函数什么也不干，如果某个体系支持JIT，就定义自己的实现，编译后体系自己实现的非`weak`版本会替换掉core.c中`__weak`修饰的这个版本，比如x86版本：
```
arch/x86/net/bpf_jit_comp.c
2246 struct bpf_prog *bpf_int_jit_compile(struct bpf_prog *prog)
2247 {
2248     struct bpf_binary_header *header = NULL;
2249     struct bpf_prog *tmp, *orig_prog = prog;
2250     struct x64_jit_data *jit_data;
2251     int proglen, oldproglen = 0;
2252     struct jit_context ctx = {};
2253     bool tmp_blinded = false;
2254     bool extra_pass = false;
2255     bool padding = false;
2256     u8 *image = NULL;
2257     int *addrs;
2258     int pass;
2259     int i;
2260

```
JIT之后,bpf_func会被替换掉：
```
arch/x86/net/bpf_jit_comp.c
        prog->bpf_func = (void *)image;
        prog->jited = 1;
        prog->jited_len = proglen;
```

## 运行

以kprobe为例，
```
trace_call_bpf
bpf_prog_run
bpf_dispatcher_nop_func(ctx, prog->insnsi, prog->bpf_func)
```
