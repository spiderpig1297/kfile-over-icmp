#include "payload_generator.h"

size_t DEFAULT_PAYLOAD_CHUNKS_SIZE = 32; // in bytes

int generate_payload(char *buffer, size_t *length)
{
    if (NULL == buffer || NULL == length) {
        return -EINVAL;
    }

    printk(KERN_DEBUG "reached get_payload()!\n");
    char array[2] = { 0xdd, 0xed};
    memcpy((void*)buffer, array, 2);
    *length = 2;
    
    return 0;
}

void setup_payload_generator()
{
    // TODO: make atomic!
    get_payload_func = &generate_payload;
}

size_t get_default_payload_chunk_size(void)
{
    return DEFAULT_PAYLOAD_CHUNKS_SIZE;
}