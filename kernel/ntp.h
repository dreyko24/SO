#ifndef VORTEX_NTP_H
#define VORTEX_NTP_H
#include <stdint.h>
/* Parse only an NTPv3/v4 server reply that corresponds to our request.
 * No dynamic allocation or libc dependencies: used in i386 and x86_64 kernels. */
static inline int vortex_ntp_reply(const uint8_t *p, int n,
                                    const uint8_t origin[8], uint64_t *unix_seconds){
    if(!p||!origin||!unix_seconds||n<48) return -1;
    /* Leap indicator 3 means unsynchronised; accept server mode 4 only. */
    if((p[0]>>6)==3 || (p[0]&7)!=4 || (p[0]&0x38)<0x18 || (p[0]&0x38)>0x20) return -1;
    if(p[1]==0 || p[1]>15) return -1;
    for(int i=0;i<8;i++) if(p[24+i]!=origin[i]) return -1;
    uint32_t sec=((uint32_t)p[40]<<24)|((uint32_t)p[41]<<16)|((uint32_t)p[42]<<8)|p[43];
    if(!sec) return -1;
    /* The NTP era wraps in 2036; era 1 covers 2036-2104. */
    uint64_t ntp_seconds=(uint64_t)sec+(sec<2208988800u?4294967296ull:0ull);
    uint64_t epoch=ntp_seconds-2208988800ull;
    if(epoch<1577836800ull || epoch>=4260211200ull) return -1; /* 2020..2104 */
    *unix_seconds=epoch;
    return 0;
}
#endif
