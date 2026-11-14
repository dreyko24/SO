/* Pruebas contra metadatos FAT32 corruptos sin acceder a hardware. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/fat32_layout.h"
static void p16(uint8_t *p,uint16_t n){p[0]=n&255;p[1]=n>>8;}
static void p32(uint8_t *p,uint32_t n){p16(p,n&65535);p16(p+2,n>>16);}
static void valid(uint8_t *s){
    memset(s,0,512);s[510]=0x55;s[511]=0xaa;
    p16(s+11,512);s[13]=8;p16(s+14,32);s[16]=2;
    p32(s+32,230000);p32(s+36,256);p32(s+44,2);
}
static void bad(const uint8_t *s,uint32_t cap,uint32_t start,uint32_t len){
    VortexFat32Layout v;assert(vortex_fat32_layout(s,cap,start,len,&v)!=0);
}
int main(void){
    uint8_t s[512];VortexFat32Layout v;
    valid(s);
    assert(vortex_fat32_layout(s,500000,2048,230000,&v)==0);
    assert(v.volume_end==232048&&v.fat_lba==2080&&v.first_data==2592);
    assert(v.cluster_count==28682&&v.root_cluster==2);
    assert(vortex_fat32_layout(s,230000,0,230000,&v)==0);
    bad(s,200000,2048,230000); /* MBR fuera del disco */
    bad(s,500000,2048,220000); /* BPB sale de la particion */
    bad(s,UINT32_MAX,UINT32_MAX-100,200); /* overflow */
    s[510]=0;bad(s,500000,2048,230000);valid(s);
    p16(s+11,4096);bad(s,500000,2048,230000);valid(s);
    s[13]=3;bad(s,500000,2048,230000);valid(s);
    s[13]=129;bad(s,500000,2048,230000);valid(s);
    p16(s+17,16);bad(s,500000,2048,230000);valid(s);
    p16(s+22,1);bad(s,500000,2048,230000);valid(s);
    p32(s+44,50000);bad(s,500000,2048,230000);valid(s);
    p32(s+36,1);bad(s,500000,2048,230000);valid(s);
    p32(s+36,0xffffffffu);bad(s,500000,2048,230000);valid(s);
    p32(s+32,100);bad(s,500000,2048,230000);valid(s);
    s[16]=3;bad(s,500000,2048,230000);valid(s);
    p32(s+44,28684);bad(s,500000,2048,230000);
    puts("FAT32: limites de volumen y clusters, overflow y campos corruptos OK");
    return 0;
}
