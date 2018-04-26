#define pr_fmt(fmt) "XX kaper_handlers: " fmt
#include "kaper_handlers.h"
#include <linux/kernel.h>
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

fp_kvm_vmx_exit_handlers* kvm_handler;
fp_kvm_vmx_exit_handlers bkp_handler[WILD_GUESS];
EXPORT_SYMBOL(bkp_handler);

unsigned long handler_virt_base = 0xffffffffa0000000;
EXPORT_SYMBOL(handler_virt_base);

void enable_page_protection(void);
void disable_page_protection(void);

// Voodoo:
// http://stackoverflow.com/questions/2103315/linux-kernel-system-call-hooking-example/4000943#4000943
void disable_page_protection(void) {

    unsigned long value;
    asm volatile("mov %%cr0,%0" : "=r" (value));
    if (value & 0x00010000) {
        value &= ~0x00010000;
        asm volatile("mov %0,%%cr0": : "r" (value));
    }
}

void enable_page_protection(void) {

    unsigned long value;
    asm volatile("mov %%cr0,%0" : "=r" (value));
    if (!(value & 0x00010000)) {
        value |= 0x00010000;
        asm volatile("mov %0,%%cr0": : "r" (value));
    }
}

int init_kvm_handler(void) {
    int i;

    pr_info("initializing kvm handlers (rodata @ %lx)\n", handler_virt_base);

    for(i=0; i<WILD_GUESS; ++i) {
        bkp_handler[i] = NULL;
    }

    ///////////////////////////////////////////////////////////////////////////
    // get the original kvm handler structure and map it into this module
    // TODO: FIX THIS!
    // you can get the actual module load address form
    // /sys/module/<module-name>/sections/.rodata
    // and the symbol from the kvm-intel.ko elf file
    // add them together respecting the page offset and such
    //follow_pte(current->mm, 0xffffffffa009f000, &ptep);
    ////follow_pte(current->mm, 0xffffffffa00b7000, &ptep);
    //if(!ptep.pte) {
    //    printk(KERN_ERR "Error finding pte for kvm exit handlers\n");
    //    return -1;
    //}
    //printk("ptep %lx\n", ptep.pte);
    //// you could also just write to the original address without remapping
    //// of course but i was initially hoping to solve this in a cleaner way
    //page = pte_page(ptep);
    //if(!page) {
    //    printk(KERN_ERR "Error mapping page for kvm exit handlers\n");
    //    return -1;
    //}
    //kvm_handler = (fp_kvm_vmx_exit_handlers*)(kmap(page) + 800);
    // TODO: if the host kernel image changes you have to fixup those offsets
    // the first one you get from the symbol offset of kvm_vmx_exit_handlers from kvm-intel.ko
    // the second one is the page offset of the .rodata section (last 3 bytes)
    //
    // README: get the symbol offset via readelf -a kvm-amd.ko   (look for svm_exit_handlers)
    //                                   readelf -a kvm-intel.ko (look for kvm_vmx_exit_handlers)
    //         the column befor the symbol name will tell you which section it will be loaded into
    //         which is .rodata in our case
    //         the loading address of the modules .rodata section can be obtained from
    //         /sys/module/<module-name>/sections/.rodata
    //         add the symbol offset to the section load address and recompile
    //pte_off = 0x340 + 0xf80;
    //kvm_handler = (fp_kvm_vmx_exit_handlers*)(kmap(page) + pte_off);
    //kvm_handler = (fp_kvm_vmx_exit_handlers*)(0xffffffffa009f000 + pte_off);
    // KASLR
    //kvm_handler = (fp_kvm_vmx_exit_handlers*)(0xffffffffc0293540 + 0x300);
    // VM
    //kvm_handler = (fp_kvm_vmx_exit_handlers*)(0xffffffffc04ba040 + 0x320);
    // AMD
    kvm_handler = (fp_kvm_vmx_exit_handlers*)(handler_virt_base + 0x340);
    pr_info("Found handler structure @%p (%p, %p, vmmcall handler: %p)\n",
            kvm_handler, kvm_handler[0], kvm_handler[1],kvm_handler[SVM_EXIT_VMMCALL]);

    return 0;
}

int reset_kvm_handler(void) {
   return 0;
}

int set_kvm_handler(int id, fp_kvm_vmx_exit_handlers new_handler) {
   return 0;
}

int unset_kvm_handler(int id) {
    disable_page_protection();
    if(bkp_handler[id])
        kvm_handler[id] = bkp_handler[id];
    enable_page_protection();
    return 0;
}
