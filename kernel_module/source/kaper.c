#define pr_fmt(fmt) "XX kaper: " fmt
#include <linux/init.h>
#include <linux/module.h>

#include "kaper.h"
#include "ioctl.h"
#include "kaper_vcpu_takeover.h"
#include "syscall_tracer_pt.h"
#include "slot_to_syscall_debugfs.h"
#include "capture_debugfs.h"
#include "kaper_debugfs.h"
#include "mem_tracer.h"
#include "kaper_handlers.h"
#include "kaper_stage_trace.h"
#include "kaper_stage_capture.h"

MODULE_LICENSE("GPL v2");

#define DRIVERNAME "kaper"

int kaper_stage;
EXPORT_SYMBOL(kaper_stage);

static struct kvm_vcpu* kapered_vcpu;

static long kaper_ioctl(struct file *, unsigned int, unsigned long);

static const struct file_operations kaper_fops = {
    .unlocked_ioctl = kaper_ioctl,
};

static struct miscdevice kaper_misc = {
    MISC_DYNAMIC_MINOR,
    "kaper",
    &kaper_fops,
};

static long kaper_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
    u64 newh = handler_virt_base;
    struct target_data td;
    struct target_data_capture tdc;
    void* user_target_data;
    void __user *argp = (void __user *)arg;
    long ret = -EIO;

    //flags = arch_local_irq_save();

    switch(cmd) {
        case KAPER_IOCTL_UPDATE_H:
            newh += arg;
            pr_info("Updating kvm module ro from %lx to %llx (+%lx)\n",
                    handler_virt_base, newh, arg);
            handler_virt_base = newh;
            ret = 0;
            break;
        case KAPER_IOCTL_KAPER:
            pr_info("Kapering vcpu\n");
            kapered_vcpu = get_vcpu((pid_t)arg);
            if(!kapered_vcpu) {
                ret = -1;
                break;
            }
            pr_info("Kapered vcpu @ %p with root_hpa: 0x%llx\n",
                    kapered_vcpu, kapered_vcpu->arch.mmu.root_hpa);
            ret = 0;
            break;
        case KAPER_IOCTL_TRACE_CR3:
            if(!kapered_vcpu) {
                pr_err("you need to kaper a process first\n");
                ret = -1;
                break;
            }
            ret = init_mem_trace(kapered_vcpu);
            break;
        case KAPER_IOCTL_UNTRACE_CR3:
            if(!kapered_vcpu) {
                pr_err("you need to kaper a process first\n");
                ret = -1;
                break;
            }
            reset_mem_trace(kapered_vcpu);
            pr_info("Memory Trace stopped\n");
            ret = 0;
            break;
        case KAPER_IOCTL_TRACE_SYSCALLS_PT:
            if(!kapered_vcpu) {
                pr_err("you need to kaper a process first\n");
                ret = -1;
                break;
            }
            pr_info("Tracing Syscalls now\n");
            init_syscall_pt_trace(kapered_vcpu);
            ret = 0;
            break;
        case KAPER_IOCTL_UNTRACE_SYSCALLS_PT:
            if(!kapered_vcpu) {
                pr_err("you need to kaper a process first\n");
                ret = -1;
                break;
            }
            pr_info("Not Tracing syscalls now\n");
            reset_syscall_pt_trace(kapered_vcpu);
            ret = 0;
            break;
       case KAPER_IOCTL_SET_STAGE_TRACE:
            kaper_stage = KAPER_TRACE;
            if (copy_from_user(&td, argp, sizeof(struct target_data))) {
                pr_err("could not copy data from user\n");
                break;
            }
            user_target_data = kmalloc(td.len + 1, GFP_KERNEL);
            if(!user_target_data) {
                pr_err("could not alloc data from user\n");
                break;
            }
            if (copy_from_user(user_target_data, (void __user*)td.ptr, td.len)) {
                pr_err("could not copy data from user\n");
                break;
            }
            set_stage_trace((char*)user_target_data, td.len);
            kfree(user_target_data);
            ret = 0;
            break;
       case KAPER_IOCTL_SET_STAGE_CAPTURE:
            kaper_stage = KAPER_CAPTURE;
            ret = 0;
            break;
       case KAPER_IOCTL_SET_CAPTURE_SEQ:
            if (copy_from_user(&tdc, argp, sizeof(struct target_data_capture))) {
                pr_err("could not copy data from user\n");
                break;
            }
            user_target_data = kmalloc(tdc.len * sizeof(int), GFP_KERNEL);
            if(!user_target_data) {
                pr_err("could not alloc data from user\n");
                break;
            }
            if (copy_from_user(user_target_data, (void __user*)tdc.ptr, tdc.len * sizeof(int))) {
                pr_err("could not copy data from user\n");
                break;
            }
            add_target_sequence(
                  (const int*)user_target_data,
                  tdc.len, tdc.typ, tdc.offset,
                  tdc.maxbt, tdc.maxskip, tdc.maxskip_head);
            kfree(user_target_data);
            ret = 0;
            break;
       case KAPER_IOCTL_SET_STAGE_REPLAY:
            // check whether a page has been captured and not replayed yet
            if(get_capture_status() && !get_replay_status()) {
               kaper_stage = KAPER_REPLAY;
               ret = 0;
            } else {
               pr_info("could not activeate replay due to a lack of replay pages\n");
               ret = -EFAULT;
            }
            break;

        default:
            pr_info("UNKNOWN IOCTL NR %d\n", cmd);
    }

    //arch_local_irq_restore(flags);
    return ret;
}

static int __init kaper_entry_init(void) {
    const char* sme_status[] = {"active","inactive"};
    if(misc_register(&kaper_misc)) {
        return -ENODEV;
    }

    pr_info("%s: loaded. SME status: %s,  x86_phys_bits: %d\n",DRIVERNAME,
            sme_active() ? sme_status[0] : sme_status[1],
            boot_cpu_data.x86_phys_bits);

    debugfs_init();
    slot_to_syscall_debugfs_init();
    capture_debugfs_init();
    return 0;
}

static void __exit kaper_entry_exit(void) {
   if(kapered_vcpu) {
      reset_syscall_pt_trace(kapered_vcpu);
      reset_mem_trace(kapered_vcpu);
   }

    slot_to_syscall_debugfs_destroy();
    capture_debugfs_destroy();
    debugfs_destroy();
    misc_deregister(&kaper_misc);
}

MODULE_LICENSE("GPL v2");
module_init(kaper_entry_init);
module_exit(kaper_entry_exit);
