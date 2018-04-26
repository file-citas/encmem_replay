#pragma once
#include <linux/list.h>
#include <linux/hashtable.h>
#include "../kernel_components/copied.h"

// for each guest page,
// this structure holds the head of a list of syscalls
// that preceeded a write to this page
struct gfn_to_syscall_ht {
    struct hlist_node hlist;
    // head of syscall list
    struct list_head sc_list;
    // lenght of system call list
    size_t sc_len;
    // guest frame number of this page
    gfn_t gfn; // <- also hash index
};

// structure to store a list of system calls
// preceeding a write to a guest page
struct sc_list {
   size_t id;
   bool match;
   struct list_head list;
};

// get guest page hash table entry given the guest frame number
// or NULL if no entry was found
struct gfn_to_syscall_ht* get_gfn2sc(gfn_t gfn, unsigned long* flags);
void put_gfn2sc(unsigned long* flags);

struct replay_page {
    u64 pfn;
    void* data;
    struct hlist_node hlist;
};

struct process_access_list {
    u64 rax;
    unsigned long check;
    unsigned long offset;
    // which cr3 value is this list for
    u64* cr3;
    // how many elements are in this list
    size_t* len;
    // counts how often the password has been located in memory
    size_t* match_index;
    struct list_head list;
    struct hlist_node hlist;
};

// access_list[gfn][cr3] -> {0, 1, 0, 10, ... }
struct access_list {
    // the mapping of pfn to gfn might change
    u64* gfn;
    DECLARE_HASHTABLE(pal, 8);
    struct list_head list;
    struct hlist_node hlist;
};

struct list_head* find_dbl(unsigned long cr3);

int init_mem_trace(struct kvm_vcpu *vcpu);
int reset_mem_trace(struct kvm_vcpu *vcpu);
size_t mem_checkpoint(struct kvm_vcpu* vcpu, int sc_id);
bool get_capture_status(void);
bool get_replay_status(void);
