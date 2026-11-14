#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/ntp.h"
static int failures;
static void expect(int cond,const char *test){ if(!cond){ fprintf(stderr,"NTP FAIL: %s\n",test); failures++; } }
static void write32(uint8_t *p,uint32_t v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
int main(void){
    uint8_t request[8]={0x83,0x15,0x37,0x42,0x11,0x22,0x33,0x44};
    uint8_t reply[48]={0}; uint64_t unix_time=0;
    reply[0]=0x24; reply[1]=2; memcpy(reply+24,request,8);
    write32(reply+40,(uint32_t)(1794650400ull+2208988800ull));
    expect(vortex_ntp_reply(reply,sizeof reply,request,&unix_time)==0,"valid NTP server response");
    expect(unix_time==1794650400ull,"UTC timestamp 2026");
    reply[24]^=1;
    expect(vortex_ntp_reply(reply,sizeof reply,request,&unix_time)<0,"reject replay / wrong origin");
    reply[24]^=1; reply[0]=0x23;
    expect(vortex_ntp_reply(reply,sizeof reply,request,&unix_time)<0,"reject client mode");
    reply[0]=0xe4;
    expect(vortex_ntp_reply(reply,sizeof reply,request,&unix_time)<0,"reject unsynchronized leap state");
    reply[0]=0x24; reply[1]=0;
    expect(vortex_ntp_reply(reply,sizeof reply,request,&unix_time)<0,"reject kiss of death");
    reply[1]=2;
    expect(vortex_ntp_reply(reply,47,request,&unix_time)<0,"reject short datagram");
    /* Era 1: post-2036 timestamps must not roll back to 1900. */
    write32(reply+40,(uint32_t)(2240611200ull+2208988800ull)); /* 2041-01-01 */
    expect(vortex_ntp_reply(reply,sizeof reply,request,&unix_time)==0,"accept NTP era 1");
    expect(unix_time==2240611200ull,"NTP era 1 conversion");
    write32(reply+40,0);
    expect(vortex_ntp_reply(reply,sizeof reply,request,&unix_time)<0,"reject zero timestamp");
    if(failures) return 1;
    puts("NTP parser: 9/9 checks OK");
    return 0;
}
