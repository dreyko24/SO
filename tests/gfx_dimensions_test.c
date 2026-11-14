/* Regresiones de dimensiones/pitch y clipping del framebuffer Vortex. */
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../kernel/gfx.h"
void *kmalloc(size_t n){return malloc(n);}
void kfree(void *p){free(p);}
int main(void){
    uint32_t front[25];memset(front,0,sizeof front);
    gfx_set_bpp(32);
    assert(gfx_init(front,0,4,16)==-1);
    assert(gfx_init(front,4,4,15)==-1);
    assert(gfx_init(front,4,4,12)==-1);
    assert(gfx_init(front,INT_MAX,4,16)==-1);
    assert(gfx_init(NULL,4,4,16)==-1);
    assert(gfx_init(front,4,4,16)==0);
    assert(gw==4&&gh==4&&bb!=NULL);
    bb[0]=0x123456;bb[15]=0x345678;
    gfx_present_rect(0,0,4,4);
    assert(front[0]==0x123456&&front[15]==0x345678);
    gfx_present_rect(INT_MIN,INT_MIN,INT_MAX,INT_MAX);
    gfx_present_rect(INT_MAX,1,12,20);
    gfx_present_rect(-8,-8,5,5);
    assert(gfx_resize(front,4,4,12)==-1);
    assert(gw==4&&gh==4&&bb[0]==0x123456);
    uint32_t *old=bb;
    gfx_set_bpp(24);
    assert(gfx_resize(front,4,4,11)==-1);
    assert(bb==old);
    assert(gfx_resize(front,4,4,12)==0);
    bb[0]=0x123456;
    gfx_present_rect(0,0,1,1);
    const unsigned char *b=(const unsigned char *)front;
    assert(b[0]==0x56&&b[1]==0x34&&b[2]==0x12);
    kfree(bb);bb=NULL;
    puts("Framebuffer: modos rechazados, resize atomico y clipping OK");
    return 0;
}
