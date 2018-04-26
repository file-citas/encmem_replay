#ifndef CR3_TO_MEM_DEBUGFS_H
#define CR3_TO_MEM_DEBUGFS_H

#include <linux/list.h>
#include <linux/debugfs.h>

int cr3_to_mem_debugfs_init(void);
int cr3_to_mem_debugfs_destroy(void);
int cr3_to_mem_debugfs_add_file(unsigned long cr3);
int print_slot_list(struct seq_file* m, struct list_head* sl);

#endif
