eBPF是字节码，是通过eBPF 虚拟机解释执行的，为啥不能在不同kernel上“跨平台”运行？
许多BPF程序访问内核struct结构，在不同版本的内核上，这些结构的字段偏移可能会发生变化。BPF字节码仍然可以在不同的内核上执行，但它可能正在读取错误的结构偏移量并打印出错误的数据。
字节码解释执行都是没有问题的，eBPF虚拟机内和内核交互和内核版本是有关系的。

# BTF(BPF Type Format)  
[来源](https://nakryiko.com/posts/btf-dedup/)


## /sys/kernel/btf/

/sys/kernel/btf/vmlinux是内核的
/sys/kernel/btf/内的其他文件是内核模块的
```
od -h /sys/kernel/btf/vmlinux | head
0000000 eb9f 0001 0018 0000 0000 0000 a4b4 002e
0000020 a4b4 002e 5ff7 0020 0001 0000 0000 0100

ls -alh /sys/kernel/btf/vmlinux
-r--r--r-- 1 root root 5.0M Mar 27 09:02 /sys/kernel/btf/vmlinux
```
### BTF文件内容
可以通过bpftool btf dump file /sys/kernel/btf/vmlinux查看btf文件的内容：  
```
[29317] FUNC_PROTO '(anon)' ret_type_id=29 vlen=5
    'regs' type_id=29
    'map' type_id=29
    'flags' type_id=29
    'data' type_id=29
    'size' type_id=29
[29318] FUNC 'bpf_perf_event_output' type_id=29317 linkage=static
```
数据结构信息：  
```
[130] STRUCT 'task_struct' size=9664 vlen=241
    'thread_info' type_id=346 bits_offset=0
    '__state' type_id=6 bits_offset=192
    'stack' type_id=83 bits_offset=256
    'usage' type_id=505 bits_offset=320
    'flags' type_id=6 bits_offset=352
    'ptrace' type_id=6 bits_offset=384
    'on_cpu' type_id=17 bits_offset=416
    'wake_entry' type_id=351 bits_offset=448
    'cpu' type_id=6 bits_offset=576
    'wakee_flips' type_id=6 bits_offset=608
    ......
    ......
    'mce_count' type_id=17 bits_offset=41472
    'kretprobe_instances' type_id=347 bits_offset=41536
    'l1d_flush_kill' type_id=79 bits_offset=41600
    'thread' type_id=224 bits_offset=41984
```

## CO-RE是如何做到的

- 编译：记录重定位信息。编译器记录对象名称name和类型type信息，即便实际运行的kernel版本不同，要访问的结构体字段偏移有变化，也能够根据name和type在对应结构内找到。
- BPF加载：加载时，内核内BTF信息和BPF程序内的重定位信息结合起来，调整BPF代码适合当前的kernel。

## 内核要求

内核编译选项：` CONFIG_DEBUG_INFO_BTF=y`。打开后才有BTF文件：`/sys/kernel/btf/vmlinux`

## btf_custom_path & bpftool gen min_core_btf

```
bpftool gen min_core_btf /sys/kernel/btf/vmlinux  our-small-btf ./bpf.o

ls -alh our-small-btf
-rw-r--r-- 1 root root 212 Aug  7 06:18 our-small-btf
```

## 相关操作

- BPF_BTF_LOAD：bpf syscall cmd。btf文件信息加载到内核。
- btf_fops：内核fd fops。
- btf_prepare_func_args
- btf_parse: verify并解析成struct btf;结构。BTF_MAGIC: 0xeb9f:
```
od -x our-small-btf
0000000 eb9f 0001 0018 0000 0000 0000 0074 0000
0000020 0074 0000 0048 0000 0001 0000 0000 0100
```

参考文献：
https://opensource.com/article/22/9/ebpf-monitor-traffic-tracee
