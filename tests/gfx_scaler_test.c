/* Vortex OS: demostrar que el escalador sin divisiones por pixel mantiene
 * exactamente el nearest-neighbour anterior con recorte y origen inverso. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../kernel/gfx.h"
void *kmalloc(size_t n){return malloc(n);}
void kfree(void *p){free(p);}

enum {W=17,H=13,SW=5,SH=4};
static uint32_t front[W*H];
static uint8_t pixels24[SH*16],pixels32[SH*20];
static uint32_t pixelsargb[SW*SH];
static void make_pixels(void){
    for(int y=0;y<SH;y++)for(int x=0;x<SW;x++){
        uint8_t red=(uint8_t)(10+19*x+3*y);
        uint8_t green=(uint8_t)(50+7*x+13*y);
        uint8_t blue=(uint8_t)(80+11*x+17*y);
        uint8_t *p=pixels24+y*16+x*3;
        p[0]=blue;p[1]=green;p[2]=red;
        p=pixels32+y*20+x*4;
        p[0]=blue;p[1]=green;p[2]=red;p[3]=0x7f;
        pixelsargb[y*SW+x]=((uint32_t)red<<16)|((uint32_t)green<<8)|blue;
    }
}
static void check_bgr(int bits,int topdown,int dx,int dy,int dw,int dh,
                      int clipx,int clipy,int clipw,int cliph){
    const uint8_t *src=bits==24?pixels24:pixels32;
    const int bytepp=bits/8,stride=bits==24?16:20;
    memset(bb,0,W*H*4u);
    clip_base(clipx,clipy,clipw,cliph);
    blit_bgr_scaled(dx,dy,dw,dh,src,SW,SH,bits,topdown);
    for(int y=0;y<H;y++)for(int x=0;x<W;x++){
        uint32_t expected=0;
        if(x>=dx&&x<dx+dw&&y>=dy&&y<dy+dh&&
           x>=clipx&&x<clipx+clipw&&y>=clipy&&y<clipy+cliph){
            int sx=(x-dx)*SW/dw,sy=(y-dy)*SH/dh;
            if(!topdown)sy=SH-1-sy;
            const uint8_t *p=src+sy*stride+sx*bytepp;
            expected=((uint32_t)p[2]<<16)|((uint32_t)p[1]<<8)|p[0];
        }
        if(bb[y*W+x]!=expected){
            fprintf(stderr,"BGR mismatch bits=%d top=%d at (%d,%d): %08x vs %08x\n",
                    bits,topdown,x,y,bb[y*W+x],expected);
            abort();
        }
    }
}
static void check32(int dx,int dy,int dw,int dh,int clipx,int clipy,int cw,int ch){
    memset(bb,0,W*H*4u);
    clip_base(clipx,clipy,cw,ch);
    blit_scaled32(dx,dy,dw,dh,pixelsargb,SW,SH);
    for(int y=0;y<H;y++)for(int x=0;x<W;x++){
        uint32_t expected=0;
        if(x>=dx&&x<dx+dw&&y>=dy&&y<dy+dh&&
           x>=clipx&&x<clipx+cw&&y>=clipy&&y<clipy+ch){
            expected=pixelsargb[((y-dy)*SH/dh)*SW+(x-dx)*SW/dw];
        }
        assert(bb[y*W+x]==expected);
    }
}
int main(void){
    make_pixels();
    gfx_set_bpp(32);
    assert(gfx_init(front,W,H,W*4)==0);
    for(int b=24;b<=32;b+=8)for(int top=0;top<2;top++){
        check_bgr(b,top,1,1,13,11,0,0,W,H); /* upscale */
        check_bgr(b,top,2,3,3,2,0,0,W,H);  /* downscale */
        check_bgr(b,top,-2,1,16,9,2,3,12,8); /* screen clip */
        check_bgr(b,top,2,-1,4,15,4,2,6,8); /* non-integral scales */
    }
    check32(1,1,13,11,0,0,W,H);
    check32(3,4,3,2,0,0,W,H);
    check32(-2,1,16,9,2,3,12,8);
    check32(2,-1,4,15,4,2,6,8);
    kfree(bb);bb=NULL;
    puts("VORTEX GFX: escalado RGB24/RGB32 y pixel32 exacto, clipping OK");
    return 0;
}
