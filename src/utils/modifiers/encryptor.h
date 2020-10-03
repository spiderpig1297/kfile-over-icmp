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

#include <linux/types.h>
#include <linux/kernel.h>

/**
 * encrypt_data - encrypts a given buffer with a XOR encryption.
 * @buffer: data to encrypt.
 * @length: data length.
 *
 * The function acts as a data_modifier_t and can't receive the encryption key.
 * Key is taken from the configuration (config.h).
 */
int encrypt_data(char *buffer, size_t *length);
