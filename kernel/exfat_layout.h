/* Vortex OS: validated exFAT volume geometry, no allocation or I/O.
 * Freestanding 32-bit compatible; all potentially overflowing sums use u64. */
#ifndef VORTEX_EXFAT_LAYOUT_H
#define VORTEX_EXFAT_LAYOUT_H
#include <stdint.h>
typedef struct {
    uint32_t fat_lba,fat_sectors,first_data,cluster_count,root_cluster;
    uint32_t sectors_per_cluster,volume_end;
} VortexExfatLayout;
static uint32_t ex_rd32(const uint8_t *p){
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static uint64_t ex_rd64(const uint8_t *p){
    return (uint64_t)ex_rd32(p)|((uint64_t)ex_rd32(p+4)<<32);
}
static int vortex_exfat_layout(const uint8_t *boot,uint32_t blocks,uint32_t first,
                               uint32_t available,VortexExfatLayout *out){
    if(!boot||!out||first>=blocks||!available||available>blocks-first)return -1;
    for(int i=0;i<8;i++)if(boot[i+3]!="EXFAT   "[i])return -2;
    if(boot[510]!=0x55||boot[511]!=0xAA||boot[0x6c]!=9)return -3;/* 512-byte sectors */
    uint32_t shift=boot[0x6d],nfats=boot[0x6e];
    if(shift>7||nfats<1||nfats>2)return -4;
    uint32_t spc=1u<<shift;
    uint64_t volume=ex_rd64(boot+0x48);
    uint32_t fo=ex_rd32(boot+0x50),fl=ex_rd32(boot+0x54);
    uint32_t heap=ex_rd32(boot+0x58),cnt=ex_rd32(boot+0x5c),root=ex_rd32(boot+0x60);
    if(!volume||volume>available||!fo||!fl||!cnt||root<2||root>=cnt+2u)return -5;
    if(cnt>0xfffffffdU)return -5;
    if((uint64_t)fo+(uint64_t)nfats*fl>heap)return -6;
    if((uint64_t)heap+(uint64_t)spc*cnt>volume)return -7;
    if((uint64_t)fl*128u<(uint64_t)cnt+2u)return -8;
    if((uint64_t)first+fo+(uint64_t)nfats*fl>blocks)return -9;
    out->fat_lba=first+fo;out->fat_sectors=fl;out->first_data=first+heap;
    out->cluster_count=cnt;out->root_cluster=root;out->sectors_per_cluster=spc;
    out->volume_end=first+(uint32_t)volume;
    return 0;
}
#endif
