//gcc ./bpf.c -o bpf
#include <stdio.h>
#include <stdlib.h>  
#include <stdint.h>    
#include <errno.h>    
#include <linux/bpf.h>    
#include <sys/syscall.h>    
#include <linux/in.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <unistd.h>

struct ipv4_packet {
    struct ethhdr eth;
    struct iphdr iph;
    struct tcphdr tcp;
} __packed;

#define ptr_to_u64(x) ((uint64_t)x)

int bpf(enum bpf_cmd cmd, union bpf_attr *attr, unsigned int size)
{
    return syscall(__NR_bpf, cmd, attr, size);
}

#define LOG_BUF_SIZE 0x1000
char bpf_log_buf[LOG_BUF_SIZE];

int bpf_prog_load(enum bpf_prog_type type, const struct bpf_insn* insns, int insn_cnt, const char* license)
{
    union bpf_attr attr = {
        .prog_type = type,        
        .insns = ptr_to_u64(insns),    
        .insn_cnt = insn_cnt,    
        .license = ptr_to_u64(license),    
        .log_buf = ptr_to_u64(bpf_log_buf),    
        .log_size = LOG_BUF_SIZE,    
        .log_level = 7,    
 //     .prog_name = "hello-world",
    };

    return bpf(BPF_PROG_LOAD, &attr, sizeof(attr));
}

int bpf_prog_test_run(int prog_fd)
{
    struct ipv4_packet pkt_v4;//must have one to pass TEST_RUN check
    union bpf_attr attr = {
        .test.prog_fd = prog_fd,        
        .test.repeat = 1,
        .test.data_in = (unsigned long)&pkt_v4,
        .test.data_size_in = sizeof(pkt_v4),
    };

    return bpf(BPF_PROG_RUN, &attr, sizeof(attr));
}

struct bpf_insn bpf_prog[] = {
    { 0xb7, 1, 0, 0, 0 }, //r1 = 0
    { 0x73, 0xa, 0x1, 0xfffc, 0}, //*(u8*)(r10 - 4 ) = r1
    { 0xb7, 1, 0, 0, 0x0a646c72}, //r1 = b'\ndlr'
    { 0x63, 0xa, 0x1, 0xfff8, 0}, //*(u32*)(r10 - 8 ) = r1
    { 0x18, 1, 0, 0, 0x6c6c6568}, //r1 = b'lleh'
    { 0x00, 0, 0, 0, 0x6f77206f}, //b'ow o'
    //7b 1a e0 ff 00 00 00 00 *(u64 *)(r10 - 16) = r1
    { 0x7b, 0xa, 0x1, 0xfff0, 0}, 
    {0xbf, 0x1, 0xa, 0, 0}, //r1 = r10
    {0x07, 0x1, 0, 0, 0xfffffff0}, //r1 += -16
    //b7 02 00 00 08 00 00 00 r2 = 13
    {0xb7, 2, 0, 0, 0x0d},
    //85 00 00 00 06 00 00 00 call 6
    {0x85, 0, 0, 0, 0x06},
    {0xb7, 0, 0, 0, 0x1},//r0 = 1
    {0x95, 0, 0, 0, 0x0 }, //exit;
};

int main(void){
    int prog_fd = bpf_prog_load(BPF_PROG_TYPE_SOCKET_FILTER, bpf_prog, sizeof(bpf_prog)/sizeof(bpf_prog[0]), "GPL");
    //int prog_fd = bpf_prog_load(BPF_PROG_TYPE_CGROUP_SKB, bpf_prog, sizeof(bpf_prog)/sizeof(bpf_prog[0]), "GPL");
    if (prog_fd < 0) {
        perror("BPF load prog");
        exit(-1);
    }
    printf("%s", bpf_log_buf);
    printf("prog_fd: %d\n", prog_fd);
    int ret = bpf_prog_test_run(prog_fd);
    printf("%s(pid %d): ret %d\n", "run this hello world", getpid(), ret);
    return 0;
}
