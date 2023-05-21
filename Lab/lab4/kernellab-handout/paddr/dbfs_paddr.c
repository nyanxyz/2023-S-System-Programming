#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <asm/pgtable.h>

MODULE_LICENSE("GPL");

static struct dentry *dir, *output;
static struct task_struct *task;

static ssize_t read_output(struct file *fp,
                        char __user *user_buffer,
                        size_t length,
                        loff_t *position)
{
    pid_t input_pid;
    char kernel_buffer[1024];

    struct mm_struct *mm;
    struct pgd_t *pgd;
    struct p4d_t *p4d;
    struct pud_t *pud;
    struct pmd_t *pmd;
    struct pte_t *pte;

    // copy_from_user: copy data from user space to kernel space
    if (copy_from_user(kernel_buffer, user_buffer, length)) {
        return -EFAULT;
    }
    sscanf(kernel_buffer, "%u", &input_pid); // read input pid

    // pid_task: input pid_struct and pid type, return task_struct
    // find_get_pid: input pid, return pid_struct
    // PIDTYPE_PID: PID represents a process
    curr = pid_task(find_get_pid(input_pid), PIDTYPE_PID);

    if (!curr) {
        printk("Cannot find task_struct associated with pid %u\n", input_pid);
        return -EINVAL;
    }

    mm = curr->mm;
    pgd = pgd_offset(mm, mm->mmap->vm_start);
}

static const struct file_operations dbfs_fops = {
    .read = read_output,
};

static int __init dbfs_module_init(void)
{
    dir = debugfs_create_dir("paddr", NULL);
    if (!dir) {
        printk("Cannot create paddr dir\n");
        return -1;
    }

    output = debugfs_create_file("output", 0666, dir, NULL, &dbfs_fops);
    if (!output) {
        printk("Cannot create output file\n");
        return -1;
    }

	printk("dbfs_paddr module initialize done\n");
    return 0;
}

static void __exit dbfs_module_exit(void)
{
    debugfs_remove_recursive(dir);

	printk("dbfs_paddr module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
