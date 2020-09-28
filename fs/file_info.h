#pragma once

#include <linux/list.h>

struct file_info {
    const char* file_path;
    struct list_head l_head;
};