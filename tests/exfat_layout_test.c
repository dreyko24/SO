#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/exfat_layout.h"
static void p32(uint8_t *p,uint32_t n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);p[2]=(uint8_t)(n>>16);p[3]=(uint8_t)(n>>24);}
static void p64(uint8_t *p,uint64_t n){p32(p,(uint32_t)n);p32(p+4,(uint32_t)(n>>32));}
int main(void){
    uint8_t b[512]={0};VortexExfatLayout l;
    memcpy(b+3,"EXFAT   ",8);b[510]=0x55;b[511]=0xaa;
    p64(b+0x48,32768);p32(b+0x50,24);p32(b+0x54,256);
    p32(b+0x58,300);p32(b+0x5c,1000);p32(b+0x60,2);
    b[0x6c]=9;b[0x6d]=3;b[0x6e]=1;
    assert(!vortex_exfat_layout(b,32768,0,32768,&l));
    assert(l.root_cluster==2&&l.first_data==300&&l.sectors_per_cluster==8);
    assert(!vortex_exfat_layout(b,40000,100,32768,&l));
    b[0x6c]=12;assert(vortex_exfat_layout(b,32768,0,32768,&l));b[0x6c]=9;
    b[0x6d]=255;assert(vortex_exfat_layout(b,32768,0,32768,&l));b[0x6d]=3;
    p64(b+0x48,UINT64_MAX);assert(vortex_exfat_layout(b,32768,0,32768,&l));
    p64(b+0x48,32768);
    p32(b+0x50,0xfffffff0u);assert(vortex_exfat_layout(b,32768,0,32768,&l));
    p32(b+0x50,24);
    p32(b+0x60,0xffffffffu);assert(vortex_exfat_layout(b,32768,0,32768,&l));
    p32(b+0x60,2);
    p32(b+0x5c,1000000);assert(vortex_exfat_layout(b,32768,0,32768,&l));
    p32(b+0x5c,1000);
    assert(vortex_exfat_layout(b,32000,0,32000,&l));
    puts("exFAT geometry validation OK");return 0;
}
