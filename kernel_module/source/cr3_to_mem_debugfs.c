#include "mem_debugfs.h"

#include "cr3_to_mem_debugfs.h"
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
#include <linux/fs.h>
#include <linux/kernel.h>
#include "mem_tracer.h"
#include "kaper_debugfs.h"

static int cr3_to_mem_open(struct inode* inode, struct file* filp);
static int cr3_to_mem_show(struct seq_file *m, void *v);

static struct dentry* debugfs_cr3_slot = NULL;

static const struct file_operations cr3_to_mem_fops = {
    .owner = THIS_MODULE,
    .read = seq_read,
    .llseek = seq_lseek,
    .open = cr3_to_mem_open,
};

int cr3_to_mem_debugfs_init(void) {
    debugfs_cr3_slot = debugfs_create_dir("cr3_slot", debugfs_root);
    if(!debugfs_cr3_slot)
        goto fail;

    return 0;
fail:
    printk(KERN_ERR "Error creating debugfs directories\n");
    return -1;
}

int cr3_to_mem_debugfs_destroy(void) {
    return 0;
}

static int cr3_to_mem_show(struct seq_file *m, void *v) {
    return 0;
}

static int cr3_to_mem_open(struct inode* inode, struct file* filp) {
    int i = 0;
    int r;
    char *cwd;
    char *buf = (char *)kmalloc(100*sizeof(char), GFP_KERNEL);
    cwd =  d_path(&filp->f_path, buf, 100*sizeof(char));
    // REMARK: Ugly path removal,
    // since we use the filename as a key into our hashtable
    // WHY did i do this again?
    for(buf=buf; *buf != '0' && i<100; ++buf);
    r = single_open(filp, cr3_to_mem_show, buf);
    kfree(buf);
    return r;
}

int cr3_to_mem_debugfs_add_file(unsigned long cr3) {
    char fname[256];
    if(!debugfs_cr3_slot) {
        return -1;
    }
    snprintf(fname, 256, "0x%lx", cr3);
    debugfs_create_file(fname, 0644, debugfs_cr3_slot, NULL, &cr3_to_mem_fops);
    return 0;
}

int print_slot_list(struct seq_file* m, struct list_head* sl) {
    return 0;
}

