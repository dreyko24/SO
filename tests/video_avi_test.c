/* Reproductor AVI nativo: formato valido y archivos corruptos con ASan/UBSan. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../kernel/video.h"
static void p16(unsigned char *p,unsigned x){p[0]=(u8)x;p[1]=(u8)(x>>8);}
static void p32(unsigned char *p,uint32_t x){p16(p,x);p16(p+2,x>>16);}
static void ck(int x,const char *s){if(!x){fprintf(stderr,"AVI FAILED: %s\n",s);exit(1);}}
static void sample(unsigned char *p,int bits,int top){
    memset(p,0,512);
    memcpy(p,"RIFF",4);p32(p+4,164);memcpy(p+8,"AVI ",4);
    memcpy(p+12,"LIST",4);p32(p+16,128);memcpy(p+20,"hdrl",4);
    memcpy(p+24,"avih",4);p32(p+28,56);p32(p+32,250000);
    memcpy(p+88,"LIST",4);p32(p+92,52);memcpy(p+96,"strl",4);
    memcpy(p+100,"strf",4);p32(p+104,40);p32(p+108,40);
    p32(p+112,1);p32(p+116,top?0xffffffffu:1);
    p16(p+120,1);p16(p+122,bits);
    memcpy(p+148,"LIST",4);p32(p+152,16);memcpy(p+156,"movi",4);
    memcpy(p+160,"00db",4);p32(p+164,4);
    p[168]=0x12;p[169]=0x34;p[170]=0x56;p[171]=0x00;
}
int main(void){
    unsigned char d[512];VVideo v;
    sample(d,24,0);
    ck(vvideo_open(d,172,&v)==0,"AVI valido 24");
    ck(v.frames==1&&v.w==1&&v.h==1&&v.fps==4,"metadatos");
    ck(vvideo_rgb(&v,0,0,0)==0x563412,"RGB BGR");
    ck(vvideo_rgb(&v,0,1,0)==0,"pixel fuera");
    ck(vvideo_open(d,171,&v)<0,"truncado");
    sample(d,32,1);
    ck(vvideo_open(d,172,&v)==0&&v.topdown,"AVI topdown 32");
    ck(vvideo_rgb(&v,0,0,0)==0x563412,"RGB 32");
    sample(d,24,0);p32(d+16,0xffffffffu);
    ck(vvideo_open(d,172,&v)<0,"LIST overflow");
    sample(d,24,0);p32(d+152,0xfffffffeu);
    ck(vvideo_open(d,172,&v)<0,"movi overflow");
    sample(d,24,0);p32(d+116,0x80000000u);
    ck(vvideo_open(d,172,&v)<0,"height INT_MIN");
    sample(d,24,0);p32(d+112,0xffffffffu);
    ck(vvideo_open(d,172,&v)<0,"width overflow");
    sample(d,24,0);p16(d+120,0);
    ck(vvideo_open(d,172,&v)<0,"invalid planes");
    sample(d,24,0);p32(d+164,0xfffffffbu);
    ck(vvideo_open(d,172,&v)<0,"frame overflow");
    sample(d,24,0);
    ck(vvideo_open(d,172,&v)==0,"valid reload");
    v.offset[0]=UINT32_MAX;
    ck(vvideo_rgb(&v,0,0,0)==0,"damaged offset");
    /* Datos aleatorios truncados y longitudes alteradas nunca deben leer fuera. */
    uint32_t seed=0xdeadbeefu;
    for(int i=0;i<10000;i++){
        sample(d,24,0);
        seed=seed*1664525u+1013904223u;
        unsigned pos=(seed>>16)%172u;
        d[pos]=(unsigned char)(seed>>8);
        seed=seed*1664525u+1013904223u;
        int len=(int)((seed>>16)%173u);
        if(vvideo_open(d,len,&v)==0)(void)vvideo_rgb(&v,0,0,0);
    }
    puts("AVI parser: valid, truncation, overflow and mutated data OK");
    return 0;
}
