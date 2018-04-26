#ifndef KAPER_VCPU_TAKEOVER_H
#define KAPER_VCPU_TAKEOVER_H

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

struct kvm_vcpu* get_vcpu(pid_t pid);

#endif
