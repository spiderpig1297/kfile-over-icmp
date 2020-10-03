#include "encryptor.h"
#include "../../config/config.h"

typedef unsigned long enc_key_t;

int encrypt_data(char *buffer, size_t *length)
{
    int i = 0;
    enc_key_t enc_key = CONFIG_ENCRYPTION_KEY;
    unsigned char key_part = 0;
    for (; i < *length; ++i) {
        key_part = (enc_key >> (8 * (i % CONFIG_ENCRYPTION_KEY_SIZE)) & 0xFF);
        buffer[i] ^= key_part;
    }
    
    return 0;
}