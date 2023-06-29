A very simple ebpf 'hello world', to understand what it is in detail, just use bpf syscall and raw bpf byte codes.

## how to build
`gcc hello.c -o hello`
## how to run
`./hello`
## how to see the result
`bpf_printk` print the "hello world" to the buf, using bpftool prog tracelog:
`bpftool prog tracelog`


