#pragma once

#include "../net/netfilter.h"

extern size_t DEFAULT_PAYLOAD_CHUNKS_SIZE;

struct file_chunk {
    const char *data;
    size_t chunk_size;
    size_t chunk_number;
    size_t total_chunks_number;
    struct list_head *l_head;
}

// struct for storing all needed information on the file that 
// is currently being sent.
// TODO: handle a case where the file is dynamic - pos / file_path changes
struct file_data_cache {
    size_t position;
    const char *file_path;
    struct list_head *chunks;
};

int generate_payload(char *buffer, size_t *length);

void setup_payload_generator(void);

size_t get_default_payload_chunk_size(void);
