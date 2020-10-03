#include "config.h"

#ifdef CONFIG_ICMP_WHITELIST_DST_IPS
extern const char *WHITELIST_DST_IPS[] = { "192.168.1.59" };
#endif

#ifdef CONFIG_ICMP_WHITELIST_SRC_IP
extern const char *WHITELIST_SRC_IPS[];
#endif
