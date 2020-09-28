#pragma once

#include "../fs/file_info.h"

#include <linux/init.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/mutex.h>

// mutex to avoid race-conditions while accessing the list.
extern struct mutex pending_files_to_be_sent_mutex;

// list of files to send
extern struct list_head pending_files_to_be_sent;

// register and unregister the char device.
int register_input_chrdev(const char* device_name);
void unregister_input_chrdev(int major_num, const char* device_name);
