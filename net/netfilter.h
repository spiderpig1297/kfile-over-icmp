/**
 * This file is part of the khidden-file-sender module (https://github.com/spiderpig1297/khidden-file-sender).
 * Copyright (c) 2020 Idan Ofek.
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

#include <linux/module.h>
#include <linux/kernel.h> 
#include <linux/init.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>

// responsible for determining what data to send.
typedef int(*get_payload_func_t)(char *buffer, size_t *length);
extern get_payload_func_t get_payload_func;

void register_nf_hook(void);
void unregister_nf_hook(void);