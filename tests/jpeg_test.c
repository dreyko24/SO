/* Prueba del decodificador JPEG nativo de Vortex OS con una imagen real.
 * Compila en host sin usar las bibliotecas de procesamiento de imagen del SO.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "../kernel/image.h"
void *kmalloc(size_t n){return malloc(n);}
void kfree(void *p){free(p);}
int zip_inflate_zlib(const uint8_t *a,int n,uint8_t *b,int cap){
    (void)a;(void)n;(void)b;(void)cap;return -1;
}
int main(int argc,char **argv){
    if(argc!=2)return 2;
    FILE *f=fopen(argv[1],"rb");if(!f)return 3;
    fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);
    if(n<=0||n>4*1024*1024){fclose(f);return 4;}
    unsigned char *b=malloc((size_t)n);if(!b){fclose(f);return 5;}
    if(fread(b,1,(size_t)n,f)!=(size_t)n){fclose(f);free(b);return 6;}
    fclose(f);
    VImage image={0};int r=vimage_decode(b,(int)n,&image);free(b);
    if(r||image.w!=128||image.h!=96||!image.pixels){
        fprintf(stderr,"JPEG fallo: %d dimensiones %dx%d\n",r,image.w,image.h);return 7;
    }
    uint32_t rgb=image.pixels[48*image.w+64];
    int red=(rgb>>16)&255,green=(rgb>>8)&255,blue=rgb&255;
    printf("JPEG decodificado: %dx%d, pixel RGB=%d,%d,%d\n",image.w,image.h,red,green,blue);
    vimage_release(&image);
    return red>=140&&red<=180&&green>=60&&green<=100&&blue>=20&&blue<=60?0:8;
}
