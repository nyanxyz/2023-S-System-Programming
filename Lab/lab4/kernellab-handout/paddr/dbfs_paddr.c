#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/pgtable.h>

MODULE_LICENSE("GPL");

static struct dentry *dir, *output;
static struct task_struct *task;

struct packet {
    pid_t pid;
    unsigned long vaddr;
    unsigned long paddr;
};

static ssize_t read_output(struct file *fp,
                        char __user *user_buffer,
                        size_t length,
                        loff_t *position)
{
    struct packet pckt;
    pid_t pid;
    unsigned long vaddr;
    unsigned long paddr;

    unsigned long mask = (1ul << 48) - 1;

    struct mm_struct *mm;
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;

    // copy_from_user: copy data from user space to kernel space
    if (copy_from_user(&pckt, user_buffer, length)) {
        return -EFAULT;
    }

    pid = pckt.pid;
    vaddr = pckt.vaddr;

    // pid_task: input pid_struct and pid type, return task_struct
    // find_get_pid: input pid, return pid_struct
    // PIDTYPE_PID: PID represents a process
    task = pid_task(find_get_pid(pid), PIDTYPE_PID);

    if (!task) {
        return -EINVAL;
    }

    mm = task->mm;

    pgd = pgd_offset(mm, vaddr);
    if (pgd_none(*pgd) || pgd_bad(*pgd)) {
        return -EINVAL;
    }

    p4d = p4d_offset(pgd, vaddr);
    if (p4d_none(*p4d) || p4d_bad(*p4d)) {
        return -EINVAL;
    }

    pud = pud_offset(p4d, vaddr);
    if (pud_none(*pud) || pud_bad(*pud)) {
        return -EINVAL;
    }

    pmd = pmd_offset(pud, vaddr);
    if (pmd_none(*pmd) || pmd_bad(*pmd)) {
        return -EINVAL;
    }

    pte = pte_offset_kernel(pmd, vaddr);
    if (pte_none(*pte)) {
        return -EINVAL;
    }

    paddr = (pte_val(*pte) & PAGE_MASK) | (vaddr & ~PAGE_MASK);
    paddr = paddr & mask;

    pckt.paddr = paddr;

    if (copy_to_user(user_buffer, &pckt, sizeof(pckt))) {
        return -EFAULT;
    }

    return sizeof(pckt);
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

    output = debugfs_create_file("output", 0444, dir, NULL, &dbfs_fops);
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
