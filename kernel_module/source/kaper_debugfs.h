#pragma once

#define BUFSIZ 1024

int debugfs_init(void);
int debugfs_destroy(void);
extern struct dentry* debugfs_root;
