#pragma once

#include <linux/module.h>
#include <linux/kernel.h> 
#include <linux/init.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>

// responsible for determining what data to send.
typedef int(*get_payload_func_t)(char *buffer, size_t *length);
extern get_payload_func_t get_payload_func;

void register_nf_hook(void);
void unregister_nf_hook(void);