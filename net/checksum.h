#pragma once

#include <linux/types.h>

__sum16 icmp_csum(const void* icmp_layer, size_t length);
