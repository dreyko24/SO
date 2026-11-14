#ifndef VORTEX_FAT32_LAYOUT_H
#define VORTEX_FAT32_LAYOUT_H
/* Valida FAT32 usando aritmetica de 64 bits ANTES de crear direcciones LBA.
 * Puro, reutilizable por pruebas en host. Solo lectura. */
#include <stdint.h>
typedef struct {
    uint32_t volume_end, fat_lba, first_data, fat_sectors;
    uint32_t sectors_per_cluster, root_cluster, cluster_count;
} VortexFat32Layout;
static inline uint16_t vf16(const uint8_t *p){
    return (uint16_t)p[0]|((uint16_t)p[1]<<8);
}
static inline uint32_t vf32(const uint8_t *p){
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|
           ((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
/* Retorna 0 si es valido, -1 si esta corrupto o sale de la particion. */
static inline int vortex_fat32_layout(const uint8_t *s,uint32_t capacity,
    uint32_t start,uint32_t partition_sectors,VortexFat32Layout *v){
    if(!s||!v||!partition_sectors||start>=capacity||
       (uint64_t)start+partition_sectors>capacity)return -1;
    if(s[510]!=0x55||s[511]!=0xaa||vf16(s+11)!=512)return -1;
    uint32_t spc=s[13],reserved=vf16(s+14),fats=s[16];
    uint32_t fsize=vf32(s+36),root=vf32(s+44);
    uint32_t total=vf16(s+19);if(!total)total=vf32(s+32);
    if(!spc||(spc&(spc-1u))||spc>128||!reserved||
       !fats||fats>2||!fsize||vf16(s+17)||vf16(s+22)||
       !total||total>partition_sectors)return -1;
    uint64_t overhead=(uint64_t)reserved+(uint64_t)fats*fsize;
    if(overhead>=total)return -1;
    /* overhead < total (u32): la division puede hacerse en 32 bits y
     * evita depender de __udivdi3 en el kernel freestanding i386. */
    uint32_t data_sectors=total-(uint32_t)overhead;
    uint64_t clusters=data_sectors/spc;
    uint64_t fat_entries=(uint64_t)fsize*128u;
    if(!clusters||clusters>0x0ffffff5u||clusters+2u>fat_entries||
       root<2||(uint64_t)root>=clusters+2u)return -1;
    uint64_t fat=(uint64_t)start+reserved, data=(uint64_t)start+overhead;
    uint64_t end=(uint64_t)start+total;
    if(fat>=end||data>=end||end>capacity||end>UINT32_MAX)return -1;
    v->volume_end=(uint32_t)end;v->fat_lba=(uint32_t)fat;
    v->first_data=(uint32_t)data;v->fat_sectors=fsize;
    v->sectors_per_cluster=spc;v->root_cluster=root;
    v->cluster_count=(uint32_t)clusters;
    return 0;
}
#endif
