#ifndef KAPER_HANDLERS_H
#define KAPER_HANDLERS_H
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
#include "../kernel_components/copied.h"

#define WILD_GUESS 256

#ifdef TARGET_AMD
typedef int (*fp_kvm_vmx_exit_handlers)(struct vcpu_svm*);
#elif TARGET_INTEL
typedef int (*fp_kvm_vmx_exit_handlers)(struct kvm_vcpu*);
#endif
int init_kvm_handler(void);
int reset_kvm_handler(void);
int set_kvm_handler(int id, fp_kvm_vmx_exit_handlers new_handler);
int unset_kvm_handler(int id);

extern fp_kvm_vmx_exit_handlers bkp_handler[WILD_GUESS];
extern unsigned long handler_virt_base;
extern fp_kvm_vmx_exit_handlers* kvm_handler;
extern void enable_page_protection(void);
extern void disable_page_protection(void);
#endif
