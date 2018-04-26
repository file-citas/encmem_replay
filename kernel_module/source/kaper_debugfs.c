#include "kaper_debugfs.h"
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

struct dentry* debugfs_root;
EXPORT_SYMBOL(debugfs_root);

int debugfs_init(void) {
    debugfs_root = debugfs_create_dir("kaper", NULL);
    if(!debugfs_root)
        goto fail;
    return 0;
fail:
    printk(KERN_ERR "Error creating debugfs directories\n");
    return -1;
}

int debugfs_destroy(void) {
    debugfs_remove_recursive(debugfs_root);
    return 0;
}

// ???
int htoi(const char *s, unsigned long *res)
{
    int c;
    unsigned long rc;

    if ('0' == s[0] && ('x' == s[1] || 'X' == s[1]))
        s += 2;

    for (rc = 0; '\0' != (c = *s); s++) {
        if ( c >= 'a' && c <= 'f') {
            c = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            c = c - 'A' + 10;
        } else if (c >= '0' && c <= '9') {
            c = c - '0';
        } else {
            return -1;
        }
        if (rc > (ULONG_MAX/16)) {
            return -1;
        }

        rc *= 16;
        rc += (unsigned long) c;
    }
    *res = rc;
    return 0;
}

