/* image.h - Decodificador de imagenes nativo de Vortex OS.
 * Sin bibliotecas Linux: BMP 24/32 bits, PNG 8 bits sin entrelazar y PPM P6.
 */
#ifndef VORTEX_IMAGE_H
#define VORTEX_IMAGE_H
#include "vortex.h"
typedef struct {
    int w,h;
    uint32_t *pixels; /* 0x00RRGGBB */
} VImage;
int vimage_decode(const unsigned char *data,int size,VImage *out);
void vimage_release(VImage *img);
#endif
