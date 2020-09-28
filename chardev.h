#pragma once

#include <linux/init.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/mutex.h>

struct file_info {
    const char* file_path;
    struct list_head l_head;
};

// mutex to avoid race-conditions while accessing the list.
extern struct mutex files_to_send_mutex;

// list of files to send
extern struct list_head files_to_send;

// register and unregister the char device.
int register_input_chrdev(const char* device_name);
void unregister_input_chrdev(int major_num, const char* device_name);
