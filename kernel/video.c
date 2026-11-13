/* Vortex OS: lector AVI RIFF para video sin compresion RGB.
 * No utiliza ffmpeg ni el kernel de Linux. */
#include "video.h"
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
static u32 rd32(const u8 *p){return (u32)p[0]|((u32)p[1]<<8)|(u32)p[2]<<16|(u32)p[3]<<24;}
static u16 rd16(const u8 *p){return (u16)p[0]|(u16)p[1]<<8;}
static int tag(const u8 *p,const char *s){return p[0]==(u8)s[0]&&p[1]==(u8)s[1]&&p[2]==(u8)s[2]&&p[3]==(u8)s[3];}
static void walk(const u8 *d,u32 begin,u32 end,int depth,int in_movi,VVideo *v){
    if(depth>5)return;
    for(u32 p=begin;p+8<=end;){
        u32 len=rd32(d+p+4),b=p+8;
        if(len>end-b)break;
        if(tag(d+p,"LIST")&&len>=4){
            int movi=in_movi||tag(d+b,"movi");
            walk(d,b+4,b+len,depth+1,movi,v);
        }else if(tag(d+p,"avih")&&len>=40){
            u32 usec=rd32(d+b);if(usec>=1000&&usec<=1000000)v->fps=(int)(1000000u/usec);
        }else if(tag(d+p,"strf")&&len>=40){
            if(rd32(d+b)>=40){
                int w=(int)rd32(d+b+4),hh=(int)rd32(d+b+8),bits=rd16(d+b+14);
                u32 compression=rd32(d+b+16);
                int top=hh<0;if(top)hh=-hh;
                if(w>=1&&w<=1280&&hh>=1&&hh<=720&&(bits==24||bits==32)&&!compression){
                    v->w=w;v->h=hh;v->bpp=bits;v->topdown=top;
                }
            }
        }else if(in_movi && ((d[p]=='0'&&d[p+1]=='0')&&(d[p+2]=='d'&&(d[p+3]=='b'||d[p+3]=='c')))){
            if(v->frames<VVIDEO_MAX_FRAMES && v->w>0&&v->h>0){
                u32 stride=(((u32)v->w*(u32)(v->bpp/8))+3u)&~3u;
                if((unsigned long long)stride*(u32)v->h<=len){
                    int f=v->frames++;v->offset[f]=b;v->length[f]=len;
                }
            }
        }
        u32 step=len+8+(len&1u);if(step>end-p)break;p+=step;
    }
}
int vvideo_open(const unsigned char *d,int sz,VVideo *v){
    if(!d||!v||sz<80)return -1;
    memset(v,0,sizeof *v);
    if(!tag(d,"RIFF")||!tag(d+8,"AVI "))return -1;
    u32 n=rd32(d+4);if(n>(u32)sz-8)n=(u32)sz-8;
    v->data=d;v->size=sz;v->fps=15;
    /* Primero la cabecera: hay que conocer el formato antes de leer los cuadros. */
    for(u32 p=12;p+8<=n+8;){
        u32 len=rd32(d+p+4);if(len>(u32)sz-p-8)break;
        if(tag(d+p,"LIST")&&len>=4&&tag(d+p+8,"hdrl"))
            walk(d,p+12,p+8+len,1,0,v);
        p+=8+len+(len&1u);
    }
    if(!v->w||!v->h)return -1;
    for(u32 p=12;p+8<=n+8;){
        u32 len=rd32(d+p+4);if(len>(u32)sz-p-8)break;
        if(tag(d+p,"LIST")&&len>=4&&tag(d+p+8,"movi"))
            walk(d,p+12,p+8+len,1,1,v);
        p+=8+len+(len&1u);
    }
    if(v->fps<1)v->fps=15;if(v->fps>60)v->fps=60;
    return v->frames>0?0:-1;
}
uint32_t vvideo_rgb(const VVideo *v,int f,int x,int y){
    if(!v||f<0||f>=v->frames||x<0||y<0||x>=v->w||y>=v->h)return 0;
    u32 stride=(((u32)v->w*(u32)(v->bpp/8))+3u)&~3u;
    u32 off=(u32)(v->topdown?y:v->h-1-y)*stride+(u32)x*(u32)(v->bpp/8);
    if(off+3>v->length[f])return 0;
    const u8 *p=v->data+v->offset[f]+off;
    return ((u32)p[2]<<16)|((u32)p[1]<<8)|p[0];
}
