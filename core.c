#include "core.h"
#include "netfilter.h"


void start(void)
{
    register_nf_hook();
}

void stop(void)
{
    unregister_nf_hook();
}