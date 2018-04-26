#pragma once
#include <linux/types.h>

#define BROKEN_SYSCALL 0x9876543210
extern u64 guest_syscalls[];
extern u64* guest_syscalls_spte[];
extern u64 ptr_apic_timer_interrupt;
extern u64 ptr_entry_SYSCALL_64;
