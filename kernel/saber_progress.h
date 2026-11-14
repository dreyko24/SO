/* Vortex Saber: progreso persistente con versión y suma de comprobación.
 * 31 artículos almacenados como bits, examen e historial acumulado local. */
#ifndef VORTEX_SABER_PROGRESS_H
#define VORTEX_SABER_PROGRESS_H
#include <stdint.h>
typedef struct {
    uint32_t completed,best,total_correct,articles,games_won;
} VortexSaberProgress;
#define SABER_PROGRESS_SIZE 28
#define SABER_PROGRESS_OLD_SIZE 24
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
    out[0]='V';out[1]='S';out[2]='P';out[3]='2';
    saber_put32(out+4,p->completed);
    saber_put32(out+8,p->best);
    saber_put32(out+12,p->total_correct);
    saber_put32(out+16,p->articles);
    saber_put32(out+20,p->games_won);
    saber_put32(out+24,saber_hash(out,24));
}
static int saber_decode(const uint8_t *in,int len,VortexSaberProgress *out){
    if(!in||!out||len<4||in[0]!='V'||in[1]!='S'||in[2]!='P')return -1;
    int legacy=len==SABER_PROGRESS_OLD_SIZE&&in[3]=='1';
    int modern=len==SABER_PROGRESS_SIZE&&in[3]=='2';
    if(!legacy&&!modern)return -1;
    int hash_offset=legacy?20:24;
    if(saber_u32(in+hash_offset)!=saber_hash(in,hash_offset))return -1;
    uint32_t attempts=saber_u32(in+4),best=saber_u32(in+8),
             total=saber_u32(in+12),games=legacy?0:saber_u32(in+20);
    if(attempts>1000000u||best>12u||total>attempts*12u||
       games>1000000u||(attempts==0&&(best||total)))return -1;
    out->completed=attempts;out->best=best;out->total_correct=total;
    out->articles=saber_u32(in+16);out->games_won=games;return 0;
}
/* Compatibilidad con las 4/6 materias de la interfaz Vortex anterior.
 * Sus mejores puntuaciones se importan sin borrar el archivo original. */
static int saber_import_legacy_text(const uint8_t *data,int len,VortexSaberProgress *out){
    if(!data||!out||(len!=6&&len!=8)||data[len-1]!='\n')return -1;
    int subjects=len-2,earned=0,completed=0,best=0;
    for(int i=0;i<subjects;i++){
        int score=(int)data[i]-'0';
        if(score<0||score>3)return -1;
        if(score){earned+=score;completed++;if(score>best)best=score;}
    }
    int game=(int)data[subjects]-'0';
    if(game<0||game>4)return -1;
    out->completed=(uint32_t)completed;
    out->total_correct=(uint32_t)earned;
    out->best=(uint32_t)best;
    out->articles=0; /* no inventar lecturas de los 31 articulos nuevos */
    out->games_won=game?1u:0u;
    return 0;
}
static int saber_read_count(uint32_t mask){
    int count=0;while(mask){count+=(int)(mask&1u);mask>>=1;}return count;
}
static int saber_level(const VortexSaberProgress *p){
    uint32_t points=p->total_correct+saber_read_count(p->articles)*2u+p->games_won*4u;
    int level=1+(int)(points/12u);return level>10?10:level;
}
#endif
