#pragma once

#include <linux/gfp.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <asm/pgtable.h>
#include <asm/io.h>
#include <asm/irqflags.h>
#include <linux/slab.h>
#include <linux/hugetlb.h>
#include <linux/fdtable.h>
#include <linux/kvm_host.h>
#include <linux/debugfs.h>
#include <linux/list.h>
#include <asm/virtext.h>

#define KAPER_TRACE 0
#define KAPER_CAPTURE 1
#define KAPER_REPLAY 2
#ifdef DEBUG
#define DBG_PRINTF(...) printk(KERN_ERR __VA_ARGS__)
#else
#define DBG_PRINTF(...)
#endif

extern int kaper_stage;
