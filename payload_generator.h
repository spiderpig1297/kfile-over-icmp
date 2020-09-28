#pragma once

#include "netfilter.h"

extern size_t DEFAULT_PAYLOAD_CHUNKS_SIZE;

int generate_payload(char *buffer, size_t *length);

void setup_payload_generator(void);

size_t get_default_payload_chunk_size(void);
