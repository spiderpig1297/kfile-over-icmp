/*
 * This file is part of the kfile-over-icmp module (https://github.com/spiderpig1297/kfile-over-icmp).
 * Copyright (c) 2020 spiderpig1297.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <linux/init.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/mutex.h>

#include "../fs/file_metadata.h"

extern struct list_head g_requestd_files_list;
extern struct mutex g_requestd_files_list_mutex;

/**
 * register_input_chrdev - registers the module's character device.
 * @device_name: name of the device.
 * 
 * The device is used for recieving the paths of the files a user wishs to send.
 */
int register_input_chrdev(const char* device_name);

/**
 * unregister_input_chrdev - unregisters the module's character device.
 * @major_num: the major number of the device (received from register_input_chrdev).
 * @device_name: name of the device.
 */
void unregister_input_chrdev(int major_num, const char* device_name);
