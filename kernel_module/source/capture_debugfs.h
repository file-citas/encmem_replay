#pragma once

#include <linux/list.h>
#include <linux/debugfs.h>

int capture_debugfs_init(void);
int capture_debugfs_destroy(void);

