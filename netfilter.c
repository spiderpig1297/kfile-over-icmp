#include "netfilter.h"
#include "checksum.h"

#include <linux/ip.h>
#include <linux/icmp.h>
#include <linux/skbuff.h>

static struct nf_hook_ops netfilter_hook;

static void register_hook(void);
static void unregister_hook(void);

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
    struct iphdr *ip_layer = ip_hdr(skb);
    if (IPPROTO_ICMP != ip_layer->protocol) {
        // ignore any non-ICMP packets.
        return NF_ACCEPT;
    }
    
    struct icmphdr *icmp_layer = icmp_hdr(skb);
    if (ICMP_ECHOREPLY != icmp_layer->code) {
        // ignore echo request packets.
        return NF_ACCEPT;
    }

    unsigned int icmp_data_offset = sizeof(struct iphdr) + sizeof(struct icmphdr) + 8;

    __u8* icmp_data = skb->data + icmp_data_offset;
    ssize_t icmp_data_length = skb->len - icmp_data_offset;
    
    // TODO: who frees it?
    // TODO: must be in padding, otherwise the ICMP reply will fail
    char* new_data = (char*)kmalloc(32, GFP_KERNEL);
    if (NULL == new_data) {
        return NF_ACCEPT;
    }

    ssize_t icmp_and_up_size = skb->len - sizeof(struct iphdr);

    memset(new_data, 0x37, 32);
    skb_put_data_impl(skb, new_data, 32);

    // update checksum
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