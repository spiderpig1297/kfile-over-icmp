#include "core.h"
#include "net/netfilter.h"
#include "chrdev/chrdev.h"
#include "fs/payload_generator.h"

static const char* input_chrdev_name = "kinput";
static int input_chrdev_major_num;

int core_start(void)
{
    // register netfilter hook
    register_nf_hook();
    printk(KERN_DEBUG "kfile-over-icmp: registered netfilter hook\n");

    // create a character device to receive user input
    input_chrdev_major_num = register_input_chrdev(input_chrdev_name);
    if (0 > input_chrdev_major_num) {
        printk(KERN_ERR "kfile-over-icmp: failed to register character device (%d)\n", input_chrdev_major_num);
        return -EBUSY;
    }

    printk(KERN_DEBUG "kfile-over-icmp: succesfully registered character device. major: %d\n", input_chrdev_major_num);
    
    setup_payload_generator();

    return 0;
}

void core_stop(void)
{
    // remove our character device
    unregister_input_chrdev(input_chrdev_major_num, input_chrdev_name);
    printk(KERN_DEBUG "kfile-over-icmp: sucessfully unregistered character device\n");

    // remove netfilter hook
    unregister_nf_hook();
    printk(KERN_DEBUG "kfile-over-icmp: netfilter hook removed\n");
}