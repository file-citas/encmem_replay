#include "capture_debugfs.h"

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


static int capture_open(struct inode* inode, struct file* filp);
static int capture_show(struct seq_file *m, void *v);
static int replay_show(struct seq_file *m, void *v);
static int replay_open(struct inode* inode, struct file* filp);

static struct dentry* debugfs_capture_root = NULL;
static struct dentry* debugfs_capture_status = NULL;

static const struct file_operations capture_status_fops = {
    .open = capture_open,
    .read = seq_read,
    .llseek = seq_lseek,
};

static const struct file_operations replay_status_fops = {
    .open = replay_open,
    .read = seq_read,
    .llseek = seq_lseek,
};

int capture_debugfs_init(void) {
    debugfs_capture_root = debugfs_create_dir("status", debugfs_root);
    if(!debugfs_capture_root)
        goto fail;

    debugfs_capture_status = debugfs_create_file("capture", 0644,
          debugfs_capture_root, NULL, &capture_status_fops);
    debugfs_capture_status = debugfs_create_file("replay", 0644,
          debugfs_capture_root, NULL, &replay_status_fops);

    return 0;
fail:
    printk(KERN_ERR "Error creating debugfs directories\n");
    return -EFAULT;
}

int capture_debugfs_destroy(void) {
    if(!debugfs_capture_root)
        return 0;
    debugfs_remove_recursive(debugfs_capture_root);
    return 0;
}

static int replay_show(struct seq_file *m, void *v) {
   seq_printf(m, "%x\n", get_replay_status());
   return 0;
}

static int replay_open(struct inode* inode, struct file* filp) {
   return single_open(filp, replay_show, NULL);
}

static int capture_show(struct seq_file *m, void *v) {
   seq_printf(m, "%x\n", get_capture_status());
   return 0;
}

static int capture_open(struct inode* inode, struct file* filp) {
   return single_open(filp, capture_show, NULL);
}
