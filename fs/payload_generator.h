#pragma once

#include "../net/netfilter.h"

static size_t DEFAULT_PAYLOAD_CHUNKS_SIZE = 32;  // in bytes
static const char DEFAULT_NEW_FILE_SIGNATURE[] = { 0xDE, 0xAD, 0xBE, 0xEF };

struct new_file_signature {
    const char *signature;
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
