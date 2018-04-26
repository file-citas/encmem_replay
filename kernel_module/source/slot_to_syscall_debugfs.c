#include "slot_to_syscall_debugfs.h"
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
//#include "../kernel_components/mmu.h"
#include "mem_tracer.h"
#include "kaper_debugfs.h"


static int slot_to_syscall_open(struct inode* inode, struct file* filp);
static int slot_to_syscall_show(struct seq_file *m, void *v);

static struct dentry* debugfs_slot_sc = NULL;

static const struct file_operations slot_to_syscall_fops = {
    .open = slot_to_syscall_open,
    .read = seq_read,
    .llseek = seq_lseek,
};

int slot_to_syscall_debugfs_init(void) {
    debugfs_slot_sc = debugfs_create_dir("slot_sc", debugfs_root);
    if(!debugfs_slot_sc)
        goto fail;

    return 0;
fail:
    printk(KERN_ERR "Error creating debugfs directories\n");
    return -1;
}

int slot_to_syscall_debugfs_destroy(void) {
    if(!debugfs_slot_sc)
        return 0;
    debugfs_remove_recursive(debugfs_slot_sc);
    //if(debugfs_root)
    //    debugfs_slot_sc = debugfs_create_dir("slot_sc", debugfs_root);
    return 0;
}

static int slot_to_syscall_show(struct seq_file *m, void *v) {
   struct gfn_to_syscall_ht* g2s;
   struct sc_list* sc;
   unsigned long gfn_offset = 0;
   unsigned long cr3 = 0;
   char* gfn_ptr;
   char* cr3_ptr;
   char* n_ptr;
   unsigned long flags;
   unsigned long index = 0;
   int i = 0;

   gfn_ptr = m->private;
   for(cr3_ptr = gfn_ptr; *cr3_ptr != '_' && i<30; ++cr3_ptr, ++i);
   *cr3_ptr = '\0';
   if(kstrtol(gfn_ptr, 16, &gfn_offset)) {
      printk(KERN_ERR "number conversion failed for gfn '%s'\n", gfn_ptr);
   }

   ++cr3_ptr;
   for(n_ptr = cr3_ptr; *n_ptr != '_' && i<60; ++n_ptr, ++i);
   *n_ptr = '\0';
   if(kstrtol(cr3_ptr, 16, &cr3)) {
      printk(KERN_ERR "number conversion failed for cr3 '%s'\n", cr3_ptr);
   }

   n_ptr++;
   if(kstrtol(n_ptr, 16, &index)) {
      printk(KERN_ERR "number conversion failed for index '%s'\n", n_ptr);
   }

   g2s = get_gfn2sc(gfn_offset, &flags);
   if(g2s == NULL) {
      pr_err("Error finding syscall list for gfn %llx\n", (unsigned long long)gfn_offset);
      put_gfn2sc(&flags);
      return 0;
   }
   list_for_each_entry(sc, &g2s->sc_list, list) {
      if(sc->match)
         seq_printf(m, "%lx *\n", sc->id);
      else
         seq_printf(m, "%lx\n", sc->id);
   }
   put_gfn2sc(&flags);
   return 0;
}

static int slot_to_syscall_open(struct inode* inode, struct file* filp) {
   int i = 0;
   int r;
   char *cwd;
   // TODO: WHY?
   char *buf = (char *)kmalloc(100*sizeof(char), GFP_KERNEL);
   cwd =  d_path(&filp->f_path, buf, 100*sizeof(char));
   for(cwd=cwd; *cwd != '0' && i<100; ++cwd, ++i);
   r = single_open(filp, slot_to_syscall_show, cwd);
   kfree(buf);
   return r;
}

int slot_to_syscall_debugfs_add_file(unsigned long gfn, unsigned long cr3, unsigned long n) {
   char fname[256];
   if(!debugfs_slot_sc)
      return -1;
   snprintf(fname, 256, "0%lx_%lx_%lx", gfn, cr3, n);
   debugfs_create_file(fname, 0644, debugfs_slot_sc, NULL, &slot_to_syscall_fops);
   return 0;
}
