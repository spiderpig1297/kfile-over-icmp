#include "checksum.h"

__sum16 icmp_csum(const void* data, size_t len) 
{
    const __u16* p = data;
    __u32 sum = 0;

    if (len & 0x1) {
        sum = (const __u8*)(p)[len - 0x1];
    }

    len /= 2;

    while (len--) {
        sum += *p++;
        if (sum & 0xffff0000) {
            sum = (sum >> 16) + (sum & 0xffff);
        }
    }

    return (__sum16)~sum;
}
