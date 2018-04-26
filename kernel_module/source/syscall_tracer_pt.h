#ifndef SYSCALL_TRACER_PT_H
#define SYSCALL_TRACER_PT_H

#include <linux/list.h>
#include "kaper.h"
#include "mem_tracer.h"


struct syscall_pt_list {
    unsigned long rax;
    struct list_head* dbl_head;
    struct list_head list;
};

struct cr3_to_syscalls_pt {
    unsigned long cr3;
    unsigned long len;
    unsigned int last_score;
    struct list_head* scl;
};

int init_syscall_pt_trace(struct kvm_vcpu* vcpu);
int reset_syscall_pt_trace(struct kvm_vcpu* vcpu);
u64* get_sptep(u64 gpa);

#endif
