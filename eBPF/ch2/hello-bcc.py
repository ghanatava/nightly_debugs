#!/bin/python3
from bcc import BPF

program = r'''
int hello(void *ctx){
    u64 pid = (bpf_get_current_pid_tgid() >> 32) & 1;
    if (pid){
        bpf_trace_printk("hello even");
        return 0;
    } 
    bpf_trace_printk("hello odd");
    return 0;
}
'''

b = BPF(text=program)
syscall = b.get_syscall_fnname("execve")
b.attach_kprobe(event=syscall,fn_name="hello")

b.trace_print()