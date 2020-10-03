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

#include <linux/module.h>
#include <linux/kernel.h> 
#include <linux/init.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>

typedef int(*get_payload_func_t)(char *buffer, size_t *length);
extern get_payload_func_t g_get_payload_func;

/**
 * register_nf_hook - registers the netfilter hook.
 * 
 * The hook will be invoked each time an outgoing packet is about to "hit the wire".
 */
void register_nf_hook(void);

/**
 * unregister_nf_hook - unregisters the netfilter hook.
 */
void unregister_nf_hook(void);
