#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");

static struct dentry *dir, *inputdir, *ptreedir;
static struct task_struct *curr;

static char *data;
static struct debugfs_blob_wrapper *blob;

static ssize_t write_pid_to_input(struct file *fp, 
                                const char __user *user_buffer, 
                                size_t length, 
                                loff_t *position)
{
    pid_t input_pid;
    char kernel_buffer[1024];
    char temp_buffer[1024];

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

    // initialize blob data
    memset(data, 0, 1024);

    // tracing process tree from input_pid to init(1) process
    while(curr) {
        // make output format string: process_command (process_id)

        // task_struct
        // comm: process command
        // pid: process id
        // parent: parent process task_struct pointer
        
        // concatenate output string to data at the beginning
        sprintf(temp_buffer, "%s (%d)\n", curr->comm, curr->pid);
        strcat(temp_buffer, data);
        strcpy(data, temp_buffer);
        
        if (curr->pid != 1) {
            curr = curr->parent;
        } else {
            break;
        }
    }

    return length;
}

// begin of code <- file write operation
static const struct file_operations dbfs_fops = {
    .write = write_pid_to_input,
};

static int __init dbfs_module_init(void)
{
    // init module code

    dir = debugfs_create_dir("ptree", NULL);
    if (!dir) {
        printk("Cannot create ptree dir\n");
        return -1;
    }

    // debugfs_create_file
    // 1. const char *name: file name
    // 2. umode_t mode: file permission
    // 3. struct dentry *parent: parent directory
    // 4. void *data: data to pass to file operations
    // 5. const struct file_operations *fops: file operations
    inputdir = debugfs_create_file("input", 0666, dir, NULL, &dbfs_fops); // file to read input
    if (!inputdir) {
        printk("Cannot create input file\n");
        return -1;
    }

    data = (char *)kmalloc(1024 * sizeof(char), GFP_KERNEL); // allocate memory for blob_data
    blob = (struct debugfs_blob_wrapper *)kmalloc(sizeof(struct debugfs_blob_wrapper), GFP_KERNEL); // allocate memory for blob

    blob->data = data;
    blob->size = 1024 * sizeof(char);

    // debugfs_create_blob
    // 1. const char *name: file name
    // 2. umode_t mode: file permission
    // 3. struct dentry *parent: parent directory
    // 4. struct debugfs_blob_wrapper *blob: blob to write
    ptreedir = debugfs_create_blob("ptree", 0666, dir, blob); // file to write output
    if (!ptreedir) {
        printk("Cannot create ptree file\n");
        return -1;
    }
    
    printk("dbfs_ptree module initialize done\n");
    return 0;
}

static void __exit dbfs_module_exit(void)
{
    // implement exit module code

    debugfs_remove_recursive(dir);
    kfree(blob);
    kfree(data);

    printk("dbfs_ptree module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
