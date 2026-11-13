#ifndef VORTEX_VIDEO_H
#define VORTEX_VIDEO_H
#include "vortex.h"
/* AVI sin compresion, BI_RGB 24/32 bits, 1 pista de video.
 * MP4/H264/HEVC/WebM requieren decodificadores posteriores.
 */
#define VVIDEO_MAX_FRAMES 2048
typedef struct {
    const unsigned char *data;
    int size,w,h,bpp,topdown,fps,frames;
    unsigned int offset[VVIDEO_MAX_FRAMES];
    unsigned int length[VVIDEO_MAX_FRAMES];
} VVideo;
int vvideo_open(const unsigned char *data,int size,VVideo *v);
uint32_t vvideo_rgb(const VVideo *v,int frame,int x,int y);
#endif
