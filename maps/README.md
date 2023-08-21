eBPF在内核运行，通过helper函数也和内核做了一些互动，结果怎么带到用户空间来？
"maps"是用于内核和用户空间共享数据的的通用存储,支持多种类型。  

The maps are accessed from user space via BPF syscall, which has commands:

create a map with given type and attributes map_fd = bpf(BPF_MAP_CREATE, union bpf_attr *attr, u32 size) using attr->map_type, attr->key_size, attr->value_size, attr->max_entries returns process-local file descriptor or negative error

lookup key in a given map err = bpf(BPF_MAP_LOOKUP_ELEM, union bpf_attr *attr, u32 size) using attr->map_fd, attr->key, attr->value returns zero and stores found elem into value or negative error

create or update key/value pair in a given map err = bpf(BPF_MAP_UPDATE_ELEM, union bpf_attr *attr, u32 size) using attr->map_fd, attr->key, attr->value returns zero or negative error

find and delete element by key in a given map err = bpf(BPF_MAP_DELETE_ELEM, union bpf_attr *attr, u32 size) using attr->map_fd, attr->key

to delete map: close(fd) Exiting process will delete maps automatically

userspace programs use this syscall to create/access maps that eBPF programs are concurrently updating.

maps can have different types: hash, array, bloom filter, radix-tree, etc.

The map is defined by:

type

max number of elements

key size in bytes

value size in bytes

struct bpf_map {
    /* The first two cachelines with read-mostly members of which some
     * are also accessed in fast-path (e.g. ops, max_entries).
     */
    const struct bpf_map_ops *ops ____cacheline_aligned;
    struct bpf_map *inner_map_meta;
#ifdef CONFIG_SECURITY
    void *security;
#endif
    enum bpf_map_type map_type;
    u32 key_size;
    u32 value_size;
    u32 max_entries;
    u32 map_flags;
    int spin_lock_off; /* >=0 valid offset, <0 error */
    int timer_off; /* >=0 valid offset, <0 error */
    u32 id;
    int numa_node;
    u32 btf_key_type_id;
    u32 btf_value_type_id;
    struct btf *btf;
#ifdef CONFIG_MEMCG_KMEM
    struct mem_cgroup *memcg;
#endif
    char name[BPF_OBJ_NAME_LEN];
    u32 btf_vmlinux_value_type_id;
    bool bypass_spec_v1;
    bool frozen; /* write-once; write-protected by freeze_mutex */
    /* 22 bytes hole */

    /* The 3rd and 4th cacheline with misc members to avoid false sharing
     * particularly with refcounting.
     */
    atomic64_t refcnt ____cacheline_aligned;
    atomic64_t usercnt;
    struct work_struct work;
    struct mutex freeze_mutex;
    u64 writecnt; /* writable mmap cnt; protected by freeze_mutex */
};

## perf-event

struct bpf_array {
    struct bpf_map map;
    u32 elem_size;
    u32 index_mask;
    struct bpf_array_aux *aux;
    union {
        char value[0] __aligned(8);
        void *ptrs[0] __aligned(8);
        void __percpu *pptrs[0] __aligned(8);
    };
};

bpf通过helper函数bpf_perf_event_output将trace数据写入map：
- BPF_CALL_5(bpf_perf_event_output, struct pt_regs *, regs, struct bpf_map *, map, u64, flags, void *, data, u64, size)
- 调用函数__bpf_perf_event_output，该函数找到当前cpu对应的内存，struct perf_event，每个cpu都使用自己的perf_event，用cpu-id作数组下标，bpf_array.ptrs[cpu_id]
- map的每一个element是一个cpu buf
- libbpf/src/libbpf.c 
        err = bpf_map_update_elem(pb->map_fd, &map_key,
                      &cpu_buf->fd, 0);
        perf_buffer__open_cpu_buf为每个cpu分配perf_buf并mmap

对PERF_EVENT_ARRAY_MAP来说，通过MAP的api(helper函数和update_map_elem)在eBPF内核和用户空间之间传递perf_buf的标识，起的是控制通道的作用，数据传输主要还是通过共享内存。  

## rlimit_memlock

创建eBPF MAP时，会在内核内分配MAP使用的内存，这些内存是驻留在内核的（相当于memlock， 关闭了swap），创建时检查创建者user也就是用户空间程序是否有权限使用这么多“MemLock”内存。

