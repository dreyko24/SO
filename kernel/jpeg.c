/* Adaptador Vortex OS para picojpeg, decodificador JPEG de dominio publico
 * de Rich Geldreich (kernel/third_party/picojpeg.c y picojpeg.h).
 * Soporta JPEG baseline YCbCr/grises, sin bibliotecas ni kernel Linux.
 */
#include "image.h"
#include "third_party/picojpeg.h"
typedef struct {const unsigned char *data;int size,pos;} JpegReader;
static unsigned char jpeg_read(unsigned char *dst,unsigned char max,
                               unsigned char *got,void *context){
    JpegReader *r=(JpegReader*)context;
    int left=r->size-r->pos;
    if(left<0)left=0;
    int n=left<(int)max?left:(int)max;
    if(n>0)memcpy(dst,r->data+r->pos,(size_t)n);
    r->pos+=n;*got=(unsigned char)n;
    return 0;
}
int vjpeg_decode(const unsigned char *d,int size,VImage *out){
    if(!d||size<16||!out)return -1;
    out->pixels=0;out->w=out->h=0;
    JpegReader reader={d,size,0};
    pjpeg_image_info_t inf;
    unsigned char st=pjpeg_decode_init(&inf,jpeg_read,&reader,0);
    if(st||inf.m_width<1||inf.m_height<1||inf.m_width>2048||
       inf.m_height>2048||inf.m_MCUSPerRow<1||inf.m_MCUSPerCol<1||
       inf.m_MCUWidth<8||inf.m_MCUHeight<8)return -1;
    unsigned w=(unsigned)inf.m_width,h=(unsigned)inf.m_height;
    uint32_t *pixels=kmalloc((size_t)w*h*4u);
    if(!pixels)return -1;
    for(int row=0;row<inf.m_MCUSPerCol;row++){
        for(int col=0;col<inf.m_MCUSPerRow;col++){
            st=pjpeg_decode_mcu();
            if(st){kfree(pixels);return -1;}
            for(int y=0;y<inf.m_MCUHeight;y++){
                int py=row*inf.m_MCUHeight+y;if(py>=inf.m_height)continue;
                for(int x=0;x<inf.m_MCUWidth;x++){
                    int px=col*inf.m_MCUWidth+x;if(px>=inf.m_width)continue;
                    int offset=(y/8)*128+(x/8)*64+(y%8)*8+(x%8);
                    unsigned r=inf.m_pMCUBufR[offset];
                    unsigned g=inf.m_comps>1?inf.m_pMCUBufG[offset]:r;
                    unsigned b=inf.m_comps>1?inf.m_pMCUBufB[offset]:r;
                    pixels[(size_t)py*w+(unsigned)px]=(r<<16)|(g<<8)|b;
                }
            }
        }
    }
    out->pixels=pixels;out->w=inf.m_width;out->h=inf.m_height;
    return 0;
}
