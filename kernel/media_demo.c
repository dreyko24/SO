/* Muestras reales de audio y video generadas por Vortex OS sin dependencias.
 * Audio: WAV PCM 8-bit mono; Video: AVI BI_RGB 24-bit, 96x64.
 * Las muestras sirven para pruebas y no sustituyen lectura de archivos externos.
 */
#include "media_demo.h"
#include "fs.h"
static void put16(unsigned char *x,unsigned v){x[0]=(unsigned char)v;x[1]=(unsigned char)(v>>8);}
static void put32(unsigned char *x,unsigned v){put16(x,v);put16(x+2,v>>16);}
static void puttag(unsigned char *x,const char *s){memcpy(x,s,4);}
static void sample_wav(FsNode *dir){
    if(!dir||fs_find(dir,"Prueba-de-audio.wav"))return;
    enum {SAMPLES=16000,N=44+SAMPLES};
    unsigned char *b=kmalloc(N);if(!b)return;
    memset(b,0,N);puttag(b,"RIFF");put32(b+4,N-8);puttag(b+8,"WAVE");
    puttag(b+12,"fmt ");put32(b+16,16);put16(b+20,1);put16(b+22,1);
    put32(b+24,8000);put32(b+28,8000);put16(b+32,1);put16(b+34,8);
    puttag(b+36,"data");put32(b+40,SAMPLES);
    /* Pitido suave en rampa triangular de 440 Hz: se oye de verdad en AC97. */
    for(int i=0;i<SAMPLES;i++){int t=(i*440/8000)%2;int v=((i*440*2/8000)%2);
        (void)t;(void)v;int phase=(i*440*64/8000)%64;
        int wave=phase<32?phase*2:126-phase*2;
        b[44+i]=(unsigned char)(128+wave/3);
    }
    FsNode *f=fs_create(dir,"Prueba-de-audio.wav",(char*)b,N);
    if(f)f->sys=1;
    kfree(b);
}
static void sample_avi(FsNode *dir){
    if(!dir||fs_find(dir,"Prueba-de-video.avi"))return;
    enum {W=96,H=64,FRAMES=12,FRAME=W*H*3};
    unsigned total=12+12+(8+56)+(12+(8+56)+(8+40))+(12+FRAMES*(8+FRAME));
    unsigned char *b=kmalloc(total);if(!b)return;
    memset(b,0,total);
    puttag(b,"RIFF");put32(b+4,total-8);puttag(b+8,"AVI ");
    unsigned p=12;
    puttag(b+p,"LIST");put32(b+p+4,4+8+56+12+8+56+8+40);puttag(b+p+8,"hdrl");p+=12;
    puttag(b+p,"avih");put32(b+p+4,56);
    put32(b+p+8,250000);put32(b+p+24,FRAMES);put32(b+p+32,1);
    put32(b+p+40,W);put32(b+p+44,H);p+=8+56;
    puttag(b+p,"LIST");put32(b+p+4,4+8+56+8+40);puttag(b+p+8,"strl");p+=12;
    puttag(b+p,"strh");put32(b+p+4,56);
    puttag(b+p+8,"vids");puttag(b+p+12,"DIB ");
    put32(b+p+28,1);put32(b+p+32,4);put32(b+p+40,FRAMES);
    put32(b+p+44,FRAME);p+=8+56;
    puttag(b+p,"strf");put32(b+p+4,40);
    put32(b+p+8,40);put32(b+p+12,W);put32(b+p+16,H);
    put16(b+p+20,1);put16(b+p+22,24);put32(b+p+28,FRAME);p+=8+40;
    puttag(b+p,"LIST");put32(b+p+4,4+FRAMES*(8+FRAME));puttag(b+p+8,"movi");p+=12;
    for(int f=0;f<FRAMES;f++){
        puttag(b+p,"00db");put32(b+p+4,FRAME);p+=8;
        for(int y=0;y<H;y++)for(int x=0;x<W;x++){
            int i=(y*W+x)*3;b[p+i]=(unsigned char)(x*2+f*15);
            b[p+i+1]=(unsigned char)(y*3+f*9);
            b[p+i+2]=(unsigned char)((x+y+f*13)&255);
        }
        p+=FRAME;
    }
    if(p!=total){kfree(b);return;}
    FsNode *file=fs_create(dir,"Prueba-de-video.avi",(char*)b,(int)total);
    if(file)file->sys=1;
    kfree(b);
}
void vortex_media_examples(void){
    FsNode *r=fs_root();if(!r)return;
    FsNode *music=fs_resolve(r,"/Musica"),*video=fs_resolve(r,"/Videos");
    sample_wav(music);sample_avi(video);
}
