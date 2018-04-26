#include "kaper_vcpu_takeover.h"

static struct files_struct* find_qemu_task_files(struct task_struct* task, pid_t pid);
static struct files_struct*  get_qemu_task_files(pid_t pid);
static int is_vcpu_fd(char* buf);
static struct file* get_vcpu_fd_file(struct files_struct* qemu_files);

static struct files_struct* find_qemu_task_files(struct task_struct* task, pid_t pid) {
    struct task_struct *child;
    struct list_head *list;
    struct files_struct* r = NULL;

    if(task->pid == pid) {
        printk("Found name: %s, pid: [%d], state: %li\n", task->comm, task->pid, task->state);
        return task->files;
    }
    list_for_each(list, &task->children) {
        child = list_entry(list, struct task_struct, sibling);
        r = find_qemu_task_files(child, pid);
        if(r)
            return r;
    }
    return NULL;
}

static struct files_struct*  get_qemu_task_files(pid_t pid) {
    return find_qemu_task_files(&init_task, pid);
}

static int is_vcpu_fd(char* buf) {
    unsigned int l = strlen(buf);
    char* buf2 = buf + (l - strlen("kvm-vcpu"));
    if(strcmp(buf2, "kvm-vcpu") == 0)
        return 1;
    return 0;
}

static struct file* get_vcpu_fd_file(struct files_struct* qemu_files) {
    struct fdtable *files_table;
    int i=0;
    struct path files_path;
    char *cwd;
    char *buf = (char *)kmalloc(100*sizeof(char), GFP_KERNEL);

    files_table = files_fdtable(qemu_files);

    while(files_table->fd[i] != NULL) { 
        files_path = files_table->fd[i]->f_path;
        cwd = d_path(&files_path, buf, 100*sizeof(char));
        if(is_vcpu_fd(cwd)) {
            printk(KERN_INFO "Found %s\n", cwd);
            return files_table->fd[i];
        }
        i++;
    }
    return NULL;
}

struct kvm_vcpu* get_vcpu(pid_t pid) {
    struct files_struct* qemu_task_files;
    struct file* vcpu_fd_file;
    printk(KERN_INFO "Taking over vcpu for pid %d\n", pid);
    qemu_task_files = get_qemu_task_files(pid);
    if(!qemu_task_files) {
        printk(KERN_ERR "Error finding qemu task\n");
        return NULL;
    }
    vcpu_fd_file = get_vcpu_fd_file(qemu_task_files);
    if(!vcpu_fd_file) {
        printk(KERN_ERR "Error finding vcpu fd\n");
        return NULL;
    }
    return (struct kvm_vcpu*) vcpu_fd_file->private_data;
}

