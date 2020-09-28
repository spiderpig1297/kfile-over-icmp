#include "chrdev.h"

#include <linux/fs.h>
#include <linux/slab.h>

DEFINE_MUTEX(files_to_send_mutex);

static int device_open_count = 0;

// file_operation functions for the character device.
static int device_open(struct inode* inode, struct file* file);
static int device_release(struct inode* inode, struct file* file);
static ssize_t device_read(struct file *fs, char *buffer, size_t len, loff_t *offset);
static ssize_t device_write(struct file *fs, const char*buffer, size_t len, loff_t *offset);

LIST_HEAD(files_to_send);

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
    struct file_info *new_file_info = (struct file_info*)kmalloc(sizeof(struct file_info), GFP_KERNEL);
    if (NULL == new_file_info) {
        return -EIO;
    }

    // allocate space for the path and save it to our file_info struct
    char* file_path = (char*)kmalloc(len, GFP_KERNEL);
    if (NULL == file_path) {
        return -EIO;
    }
    memcpy(file_path, buffer, len);

    new_file_info->file_path = file_path;

    mutex_lock(&files_to_send_mutex);
    INIT_LIST_HEAD(&new_file_info->l_head);
    list_add_tail(&new_file_info->l_head, &files_to_send);
    mutex_unlock(&files_to_send_mutex);

    printk(KERN_INFO "kprochide: new file to send: %d\n", file_path);

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