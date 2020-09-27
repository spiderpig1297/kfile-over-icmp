#pragma once

#include <linux/module.h>
#include <linux/kernel.h> 
#include <linux/init.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>

void register_nf_hook(void);
void unregister_nf_hook(void);