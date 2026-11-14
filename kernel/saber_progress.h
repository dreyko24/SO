/* Vortex Saber: progreso persistente con versión y suma de comprobación.
 * 31 artículos almacenados como bits, examen e historial acumulado local. */
#ifndef VORTEX_SABER_PROGRESS_H
#define VORTEX_SABER_PROGRESS_H
#include <stdint.h>
typedef struct {
    uint32_t completed,best,total_correct,articles;
} VortexSaberProgress;
#define SABER_PROGRESS_SIZE 24
static uint32_t saber_u32(const uint8_t *p){
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static void saber_put32(uint8_t *p,uint32_t v){
    p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);
    p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);
}
static uint32_t saber_hash(const uint8_t *p,int n){
    uint32_t h=2166136261u;
    for(int i=0;i<n;i++)h=(h^p[i])*16777619u;
    return h;
}
static void saber_encode(const VortexSaberProgress *p,uint8_t out[SABER_PROGRESS_SIZE]){
    out[0]='V';out[1]='S';out[2]='P';out[3]='1';
    saber_put32(out+4,p->completed);
    saber_put32(out+8,p->best);
    saber_put32(out+12,p->total_correct);
    saber_put32(out+16,p->articles);
    saber_put32(out+20,saber_hash(out,20));
}
static int saber_decode(const uint8_t *in,int len,VortexSaberProgress *out){
    if(!in||!out||len!=SABER_PROGRESS_SIZE||in[0]!='V'||in[1]!='S'||
       in[2]!='P'||in[3]!='1'||saber_u32(in+20)!=saber_hash(in,20))return -1;
    uint32_t attempts=saber_u32(in+4),best=saber_u32(in+8),
             total=saber_u32(in+12);
    if(attempts>1000000u||best>12u||total>attempts*12u||
       (attempts==0&&(best||total)))return -1;
    out->completed=attempts;out->best=best;out->total_correct=total;
    out->articles=saber_u32(in+16);return 0;
}
static int saber_read_count(uint32_t mask){
    int count=0;while(mask){count+=(int)(mask&1u);mask>>=1;}return count;
}
static int saber_level(const VortexSaberProgress *p){
    uint32_t points=p->total_correct+saber_read_count(p->articles)*2u;
    int level=1+(int)(points/18u);return level>10?10:level;
}
#endif
