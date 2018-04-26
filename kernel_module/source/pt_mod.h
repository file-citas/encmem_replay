#pragma once
#include <asm/compat.h>
#include <linux/kvm_types.h>

#define EPT_READ 1
#define EPT_WRITE 2
#define EPT_EXEC 4
#define NP_NX 0x8000000000000000
#define NP_RW 0x2

void set_prot_all(u64 pflags, char set, struct kvm_vcpu* vcpu);
void prot(u64* sptep, u64 pflags, char set, struct kvm_vcpu* vcpu);
u64* get_ptptr(u64 gpa, struct kvm_vcpu* vcpu);
u64* get_ptptr_nolock(u64 gpa, struct kvm_vcpu* vcpu);
