#ifndef SLOT_TO_SYSCALL_DEBUGFS_H
#define SLOT_TO_SYSCALL_DEBUGFS_H

#include <linux/list.h>
#include <linux/debugfs.h>

int slot_to_syscall_debugfs_init(void);
int slot_to_syscall_debugfs_destroy(void);
int slot_to_syscall_debugfs_add_file(unsigned long gfn, unsigned long cr3, unsigned long n);

#endif
