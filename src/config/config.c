#include "config.h"

#ifdef CONFIG_ICMP_WHITELIST_DST_IPS
const int WHITELIST_DST_IPS_NUMBER = 1;
const char *WHITELIST_DST_IPS[] = { "192.168.1.59" };  // Don't forget to update the size above! 
#endif

#ifdef CONFIG_ICMP_WHITELIST_SRC_IPS
const int WHITELIST_SRC_IPS_NUMBER = 1;
const char *WHITELIST_SRC_IPS[] = { "192.168.1.1" };  // Don't forget to update the size above!
#endif
 