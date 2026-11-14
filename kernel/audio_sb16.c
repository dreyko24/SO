/* Sound Blaster 16, ISA DMA channel 1, 8-bit mono.
 * Hardware-driven playback with two 16 KiB halves in a 32 KiB DMA ring.
 * Operates without an OS-specific audio server; no Linux code.
 */
#include "audio_sb16.h"
#include "vortex.h"
#ifdef HOSTGUI
int sb16_probe(void){return 0;}
int sb16_play(FsNode *f){(void)f;return -1;}
void sb16_stop(void){}
void sb16_pause(int p){(void)p;}
void sb16_tick(void){}
int sb16_active(void){return 0;}
int sb16_progress(void){return 0;}
int sb16_paused(void){return 0;}
const char *sb16_error(void){return "SB16 no esta disponible en el simulador";}
#else
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
#define SB16_BASE 0x220
#define SB_RATE 22050u
#define SB_DMA_SIZE 32768
#define SB_HALF 16384
static u8 dma_raw[65536] __attribute__((aligned(65536)));
static u8 *dma_buf;
static FsNode *source_hold;
static const u8 *source;
static int detected=-1, playing, paused, channels, bits, last_half, underrun;
static u32 count_frames, src_frame, src_rate, fraction;
static const char *error="Sin SB16";
static inline void o8(u16 p,u8 v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static inline u8 i8(u16 p){u8 x;__asm__ volatile("inb %1,%0":"=a"(x):"Nd"(p));return x;}
static u16 rd16(const u8 *p){return (u16)p[0]|((u16)p[1]<<8);}
static u32 rd32(const u8 *p){return (u32)p[0]|((u32)p[1]<<8)|((u32)p[2]<<16)|((u32)p[3]<<24);}
static int eq4(const u8 *p,const char *s){return p[0]==(u8)s[0]&&p[1]==(u8)s[1]&&p[2]==(u8)s[2]&&p[3]==(u8)s[3];}
static int dsp_write(u8 x){
    for(int tries=0;tries<150000;tries++)if(!(i8(SB16_BASE+0x0c)&0x80)){o8(SB16_BASE+0x0c,x);return 0;}
    return -1;
}
static int dsp_read(void){
    for(int tries=0;tries<150000;tries++)if(i8(SB16_BASE+0x0e)&0x80)return i8(SB16_BASE+0x0a);
    return -1;
}
int sb16_probe(void){
    if(detected>=0)return detected;
    detected=0;
    o8(SB16_BASE+6,1);
    for(volatile int i=0;i<5000;i++){}
    o8(SB16_BASE+6,0);
    if(dsp_read()!=0xaa)return 0;
    if(dsp_write(0xe1))return 0;
    int maj=dsp_read(),min=dsp_read();
    if(maj<4||min<0)return 0;
    uintptr_t base=(uintptr_t)dma_raw;
    /* DMA 8237: 16MiB address ceiling and 64KiB boundary. */
    if(base>=0x1000000u||base+SB_DMA_SIZE>0x1000000u)return 0;
    dma_buf=dma_raw;
    detected=1;
    error="SB16 ISA disponible";
    kprintf("[vortex-audio] SB16 detectado (DSP %d.%d)\n",maj,min);
    return 1;
}
static int parse(FsNode *f){
    if(!f||f->dir||!f->data||f->size<44)return -1;
    const u8 *d=(const u8*)f->data;
    u32 size=(u32)f->size,offset=0,bytes=0,rate=0;
    int ch=0,bit=0,has_fmt=0;
    if(!eq4(d,"RIFF")||!eq4(d+8,"WAVE"))return -1;
    for(u32 pos=12;pos+8<=size;){
        u32 len=rd32(d+pos+4);
        if(len>size-pos-8)break;
        if(eq4(d+pos,"fmt ")&&len>=16){
            const u8 *v=d+pos+8;
            if(rd16(v)!=1)return -1;
            ch=(int)rd16(v+2);rate=rd32(v+4);bit=(int)rd16(v+14);
            has_fmt=1;
        }
        if(eq4(d+pos,"data")){offset=pos+8;bytes=len;}
        pos+=8+len+(len&1u);
    }
    if(!has_fmt||!bytes||!rate||rate<8000||rate>96000||ch<1||ch>2||(bit!=8&&bit!=16))return -1;
    int bytes_per_frame=ch*(bit/8);
    if(bytes<(u32)bytes_per_frame)return -1;
    source=d+offset;count_frames=bytes/(u32)bytes_per_frame;
    channels=ch;bits=bit;src_rate=rate;src_frame=0;fraction=0;underrun=0;
    return 0;
}
static void fill_half(int half){
    u8 *dst=dma_buf+half*SB_HALF;
    for(int i=0;i<SB_HALF;i++){
        if(src_frame>=count_frames){dst[i]=128;continue;}
        const u8 *s=source+(size_t)src_frame*(size_t)channels*(unsigned)(bits/8);
        int v;
        if(bits==8){v=(int)s[0];if(channels==2)v=(v+(int)s[1])/2;}
        else{
            int l=(short)rd16(s),r=channels==2?(short)rd16(s+2):l;
            v=128+((l+r)/2)/256;
            if(v<0)v=0;if(v>255)v=255;
        }
        dst[i]=(u8)v;
        fraction+=src_rate;
        while(fraction>=SB_RATE){fraction-=SB_RATE;src_frame++;}
    }
}
static void program_dma(void){
    u32 a=(u32)(uintptr_t)dma_buf;
    o8(0x0a,0x05);/* mask channel 1 */
    o8(0x0c,0);/* clear flip-flop */
    o8(0x0b,0x59);/* channel 1, read, auto-init */
    o8(0x83,(u8)(a>>16));
    o8(0x02,(u8)a);o8(0x02,(u8)(a>>8));
    o8(0x03,(u8)(SB_DMA_SIZE-1));o8(0x03,(u8)((SB_DMA_SIZE-1)>>8));
    o8(0x0a,0x01);
}
int sb16_play(FsNode *file){
    sb16_stop();
    if(!sb16_probe()){error="SB16 no detectado";return -2;}
    if(parse(file)){error="WAV PCM incompatible";return -1;}
    fs_node_ref(file);source_hold=file;
    fill_half(0);fill_half(1);last_half=0;
    program_dma();
    if(dsp_write(0xd1)||dsp_write(0x41)||dsp_write((u8)(SB_RATE>>8))||
       dsp_write((u8)SB_RATE)||dsp_write(0xc6)||dsp_write(0x00)||
       dsp_write((u8)(SB_HALF-1))||dsp_write((u8)((SB_HALF-1)>>8))){
        sb16_stop();error="No se pudo iniciar DMA de SB16";return -1;
    }
    playing=1;paused=0;error="Reproduciendo WAV por SB16";
    kprintf("[vortex-audio] WAV PCM por SB16: %u Hz, %u canales, %u bits\n",src_rate,(u32)channels,(u32)bits);
    return 0;
}
void sb16_stop(void){
    if(playing){(void)dsp_write(0xda);(void)dsp_write(0xd0);o8(0x0a,0x05);(void)i8(SB16_BASE+0x0e);}
    if(source_hold)fs_node_unref(source_hold);
    source_hold=0;source=0;playing=paused=last_half=underrun=0;
    count_frames=src_frame=fraction=0;
}
void sb16_pause(int p){
    if(!playing)return;
    p=!!p;if(p==paused)return;
    if(!dsp_write(p?0xd0:0xd4))paused=p;
}
int sb16_active(void){return playing;}
int sb16_paused(void){return paused;}
int sb16_progress(void){
    if(!count_frames)return 0;
    u32 r=count_frames>=100?src_frame/(count_frames/100):(src_frame*100)/count_frames;
    return r>100?100:(int)r;
}
const char *sb16_error(void){return error;}
void sb16_tick(void){
    if(!playing||paused)return;
    o8(0x0c,0);
    u32 lo=i8(0x03),hi=i8(0x03),remaining=lo|(hi<<8);
    if(remaining>=SB_DMA_SIZE)return;
    u32 position=(SB_DMA_SIZE-1-remaining)&(SB_DMA_SIZE-1u);
    int half=position>=SB_HALF?1:0;
    if(half!=last_half){
        /* Rellenar SOLO la mitad que el hardware ya termino de leer. */
        fill_half(last_half);
        last_half=half;
        (void)i8(SB16_BASE+0x0e);/* reconocer IRQ8 */
        if(src_frame>=count_frames){
            if(++underrun>=3){sb16_stop();error="Finalizado";return;}
        }else underrun=0;
    }
}
#endif
