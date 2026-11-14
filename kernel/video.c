/* Vortex OS: lector AVI RIFF de fotogramas RGB sin compresion.
 * Cada longitud se contrasta contra el tamanio real del archivo antes de usarla. */
#include "video.h"
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
static u32 rd32(const u8 *p){
    return (u32)p[0]|((u32)p[1]<<8)|((u32)p[2]<<16)|((u32)p[3]<<24);
}
static u16 rd16(const u8 *p){return (u16)p[0]|((u16)p[1]<<8);}
static int tag(const u8 *p,const char *s){
    return p[0]==(u8)s[0]&&p[1]==(u8)s[1]&&p[2]==(u8)s[2]&&p[3]==(u8)s[3];
}
/* Rechazar chunks truncados, LIST sin tipo y profundidad excesiva. */
static int walk(const u8 *d,u32 begin,u32 end,int depth,int movi,VVideo *v){
    if(depth>5||begin>end||end>(u32)v->size)return -1;
    for(u32 p=begin;p<=end&&end-p>=8;){
        u32 b=p+8u,len=rd32(d+p+4u);
        if(len>end-b)return -1;
        u32 next=b+len;
        /* RIFF alinea chunks a dos bytes: comprobar el padding tambien. */
        if(len&1u){
            if(next>=end)return -1;
            ++next;
        }
        if(tag(d+p,"LIST")){
            if(len<4)return -1;
            if(walk(d,b+4u,b+len,depth+1,movi||tag(d+b,"movi"),v))return -1;
        }else if(tag(d+p,"avih")&&len>=40){
            u32 usec=rd32(d+b);
            if(usec>=1000&&usec<=1000000)v->fps=(int)(1000000u/usec);
        }else if(tag(d+p,"strf")&&len>=40){
            if(rd32(d+b)>=40){
                u32 width=rd32(d+b+4),rawh=rd32(d+b+8);
                u32 height=(rawh&0x80000000u)?(0u-rawh):rawh;
                int bits=rd16(d+b+14);
                u32 compression=rd32(d+b+16);
                if(width>=1&&width<=1280&&height>=1&&height<=720&&
                   rd16(d+b+12)==1&&(bits==24||bits==32)&&!compression){
                    v->w=(int)width;v->h=(int)height;v->bpp=bits;
                    v->topdown=!!(rawh&0x80000000u);
                }
            }
        }else if(movi&&d[p]=='0'&&d[p+1]=='0'&&d[p+2]=='d'&&
                 (d[p+3]=='b'||d[p+3]=='c')){
            if(v->frames<VVIDEO_MAX_FRAMES&&v->w>0&&v->h>0){
                u32 stride=(((u32)v->w*(u32)(v->bpp/8))+3u)&~3u;
                if(stride*(u32)v->h<=len){
                    int f=v->frames++;v->offset[f]=b;v->length[f]=len;
                }
            }
        }
        p=next;
    }
    return 0;
}
int vvideo_open(const unsigned char *d,int sz,VVideo *v){
    if(!d||!v||sz<80)return -1;
    memset(v,0,sizeof *v);
    if(!tag(d,"RIFF")||!tag(d+8,"AVI "))return -1;
    u32 declared=rd32(d+4);
    if(declared<4||declared>(u32)sz-8u)return -1;
    u32 end=declared+8u;
    v->data=d;v->size=sz;v->fps=15;
    /* Leer formatos antes de los pixeles, incluso con LIST en distinto orden. */
    for(u32 p=12;p<=end&&end-p>=8;){
        u32 b=p+8u,len=rd32(d+p+4u);
        if(len>end-b)return -1;
        if(tag(d+p,"LIST")&&len>=4&&tag(d+b,"hdrl")){
            if(walk(d,b+4u,b+len,1,0,v))return -1;
        }else if(tag(d+p,"LIST")&&len<4)return -1;
        u32 next=b+len;
        if(len&1u){if(next>=end)return -1;++next;}
        p=next;
    }
    if(!v->w||!v->h)return -1;
    for(u32 p=12;p<=end&&end-p>=8;){
        u32 b=p+8u,len=rd32(d+p+4u);
        if(len>end-b)return -1;
        if(tag(d+p,"LIST")&&len>=4&&tag(d+b,"movi")){
            if(walk(d,b+4u,b+len,1,1,v))return -1;
        }else if(tag(d+p,"LIST")&&len<4)return -1;
        u32 next=b+len;
        if(len&1u){if(next>=end)return -1;++next;}
        p=next;
    }
    if(v->fps<1)v->fps=15;
    if(v->fps>60)v->fps=60;
    return v->frames>0?0:-1;
}
uint32_t vvideo_rgb(const VVideo *v,int f,int x,int y){
    if(!v||!v->data||v->size<=0||f<0||f>=v->frames||
       f>=VVIDEO_MAX_FRAMES||v->w<1||v->w>1280||v->h<1||v->h>720||
       (v->bpp!=24&&v->bpp!=32)||
       x<0||y<0||x>=v->w||y>=v->h)return 0;
    u32 bytes=(u32)v->bpp/8u;
    u32 stride=(((u32)v->w*bytes)+3u)&~3u;
    u32 off=(u32)(v->topdown?y:v->h-1-y)*stride+(u32)x*bytes;
    u32 base=v->offset[f],length=v->length[f];
    /* Tambien verificar la tabla de offsets, no confiar solo en la cabecera. */
    if(base>(u32)v->size||length>(u32)v->size-base||
       off>length||bytes>length-off)return 0;
    const u8 *p=v->data+base+off;
    return ((u32)p[2]<<16)|((u32)p[1]<<8)|p[0];
}
