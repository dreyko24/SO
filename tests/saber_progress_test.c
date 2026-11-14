#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/saber_progress.h"
int main(void){
    uint8_t image[SABER_PROGRESS_SIZE];
    VortexSaberProgress original={3,12,28,0x80000001u},restored={0};
    saber_encode(&original,image);
    assert(!saber_decode(image,sizeof image,&restored));
    assert(restored.best==12&&restored.completed==3&&restored.articles==0x80000001u);
    assert(saber_level(&restored)>1);
    assert(saber_read_count(restored.articles)==2);
    image[13]^=0x80;assert(saber_decode(image,sizeof image,&restored));
    image[13]^=0x80;
    assert(saber_decode(image,12,&restored));
    image[0]='X';assert(saber_decode(image,sizeof image,&restored));
    puts("VORTEX SABER PROGRESS: VERSION, HASH, RESTART AND LEVEL OK");
    return 0;
}
