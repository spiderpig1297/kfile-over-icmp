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

#include <linux/list.h>

/**
 * Prototype for modifying file's data.
 * Each function receives the file's data and is allowed to make any 
 * modification to it, as long is it doesn't kfree() it and remembers to
 * update the length argument before returning.
 */
typedef int(*data_modifier_func)(char *buffer, size_t *length);

struct data_modifier {
    data_modifier_func func;
    struct list_head l_head;
};