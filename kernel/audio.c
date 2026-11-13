/* Vortex OS - audio PCI AC'97, salida DMA PCM 48 kHz estereo 16 bit.
 * Funciona en la maquina virtual QEMU -device AC97 y en PCI AC'97 compatible.
 * Sin bibliotecas externas ni codigo del kernel Linux.
 */
#include "audio.h"
#include "vortex.h"
#ifdef HOSTGUI
static int playing, paused;
int audio_hardware(void){return 1;}
int audio_play(FsNode *file){if(!file||file->size<44)return -1;playing=1;paused=0;return 0;}
void audio_tick(void){}
int audio_active(void){return playing;}
int audio_paused(void){return paused;}
void audio_pause(int p){paused=!!p;}
void audio_stop(void){playing=0;}
int audio_progress(void){return 0;}
const char *audio_error(void){return "Audio simulado en host";}
#else
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
static inline u32 pci_rd(int b,int d,int f,int o){
    u32 a=0x80000000u|((u32)b<<16)|((u32)d<<11)|((u32)f<<8)|(o&252u);
    __asm__ volatile("outl %0,%1"::"a"(a),"Nd"((u16)0xCF8));
    u32 v;__asm__ volatile("inl %1,%0":"=a"(v):"Nd"((u16)0xCFC));return v;
}
static inline void pci_wr(int b,int d,int f,int o,u32 val){
    u32 a=0x80000000u|((u32)b<<16)|((u32)d<<11)|((u32)f<<8)|(o&252u);
    __asm__ volatile("outl %0,%1"::"a"(a),"Nd"((u16)0xCF8));
    __asm__ volatile("outl %0,%1"::"a"(val),"Nd"((u16)0xCFC));
}
static inline void o8(u16 p,u8 v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static inline void o16(u16 p,u16 v){__asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p));}
static inline void o32(u16 p,u32 v){__asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p));}
static inline u8 i8(u16 p){u8 v;__asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p));return v;}
static inline u16 i16(u16 p){u16 v;__asm__ volatile("inw %1,%0":"=a"(v):"Nd"(p));return v;}
static u16 mixio,busio; static int tried,found,playing,paused,queued,fillidx,lastciv,ending;
#define NSLOT 32
#define SLOT_BYTES 4096
typedef struct {u32 ptr;u16 len;u16 flags;} Bdl;
static Bdl bdl[NSLOT] __attribute__((aligned(16)));
static u8 pcm[NSLOT][SLOT_BYTES] __attribute__((aligned(16)));
static const u8 *src;
static FsNode *hold;
static u32 size,rate,frames,srcidx,frac,datarate;
static int channels,bits;
static const char *err="Sin dispositivo AC'97";
static int eq4(const u8 *p,const char *s){for(int i=0;i<4;i++)if(p[i]!=(u8)s[i])return 0;return 1;}
static u16 rd16(const u8 *x){return (u16)x[0]|((u16)x[1]<<8);}
static u32 rd32(const u8 *x){return (u32)x[0]|((u32)x[1]<<8)|((u32)x[2]<<16)|((u32)x[3]<<24);}
int audio_hardware(void){
    if(tried)return found;
    tried=1;
    for(int b=0;b<256;b++)for(int d=0;d<32;d++){
        if((pci_rd(b,d,0,0)&65535u)==65535u)continue;
        int nf=(pci_rd(b,d,0,12)&0x00800000u)?8:1;
        for(int f=0;f<nf;f++){
            u32 id=pci_rd(b,d,f,0);
            if((id&65535u)==65535u)continue;
            u32 cls=pci_rd(b,d,f,8);
            if((cls>>16)!=0x0401u)continue;
            u32 bar0=pci_rd(b,d,f,0x10),bar1=pci_rd(b,d,f,0x14);
            if(!(bar0&1)||!(bar1&1))continue;
            mixio=(u16)(bar0&~3u);busio=(u16)(bar1&~3u);
            if(!mixio||!busio)continue;
            pci_wr(b,d,f,4,pci_rd(b,d,f,4)|5u); /* IO + bus master */
            o16(mixio+0,0); /* reset mixer */
            for(volatile int wait=0;wait<100000;wait++){}
            o16(mixio+2,0); /* master: maximo volumen, sin mute */
            o16(mixio+0x18,0); /* PCM out */
            if(i16(mixio+0x28)&1)o16(mixio+0x2a,i16(mixio+0x2a)|1);
            o16(mixio+0x2c,48000);
            found=1;err="Listo";kprintf("[vortex-audio] AC97 detectado\n");
            return 1;
        }
    }
    return 0;
}
void audio_stop(void){
    if(found){o8(busio+0x1b,0);o8(busio+0x1b,2);o8(busio+0x1b,0);o16(busio+0x16,0x001c);}
    if(hold)fs_node_unref(hold);
    hold=0;src=0;playing=paused=queued=fillidx=lastciv=ending=0;frames=size=srcidx=frac=0;
}
static int parse(FsNode *file){
    if(!file||file->dir||!file->data||file->size<44)return -1;
    const u8 *d=(const u8*)file->data;u32 n=(u32)file->size;
    if(!eq4(d,"RIFF")||!eq4(d+8,"WAVE"))return -1;
    int fmt=0;const u8 *data=0;u32 bytes=0;
    for(u32 p=12;p+8<=n;){
        u32 l=rd32(d+p+4);if(l>n-p-8)break;
        if(eq4(d+p,"fmt ")&&l>=16){
            const u8 *v=d+p+8;
            if(rd16(v)!=1)return -1;
            channels=rd16(v+2);rate=rd32(v+4);bits=rd16(v+14);
            fmt=1;
        }
        if(eq4(d+p,"data")){data=d+p+8;bytes=l;}
        p+=8+l+(l&1);
    }
    if(!fmt||!data||channels<1||channels>2||(bits!=8&&bits!=16)||rate<8000||rate>96000)return -1;
    int framebytes=channels*(bits/8);
    if(bytes<(u32)framebytes)return -1;
    src=data;size=bytes;frames=bytes/(u32)framebytes;
    srcidx=0;frac=0;datarate=48000;
    return 0;
}
static int refill(int idx){
    if(srcidx>=frames)return 0;
    int bytes=0; u8 *dst=pcm[idx];
    while(bytes+4<=SLOT_BYTES && srcidx<frames){
        const u8 *x=src+(size_t)srcidx*channels*(bits/8);
        int l,r;
        if(bits==8){l=((int)x[0]-128)<<8;r=channels==2?((int)x[1]-128)<<8:l;}
        else{l=(short)rd16(x);r=channels==2?(short)rd16(x+2):l;}
        dst[bytes++]=(u8)l;dst[bytes++]=(u8)((u16)l>>8);
        dst[bytes++]=(u8)r;dst[bytes++]=(u8)((u16)r>>8);
        frac+=rate;
        while(frac>=48000){frac-=48000;srcidx++;}
    }
    if(bytes==0)return 0;
    bdl[idx].ptr=(u32)(uintptr_t)dst;
    bdl[idx].len=(u16)(bytes/2);
    bdl[idx].flags=0; /* BUP normal, no interrumpir al kernel */
    return 1;
}
int audio_play(FsNode *file){
    audio_stop();if(!audio_hardware()){err="No hay tarjeta AC'97 compatible";return -2;}
    if(parse(file)){err="Formato no compatible: requiere WAV PCM 8/16-bit";return -1;}
    fs_node_ref(file);hold=file;
    for(int i=0;i<NSLOT;i++){if(!refill(i))break;queued++;fillidx=(i+1)&31;}
    if(!queued){audio_stop();err="Archivo de audio vacio";return -1;}
    o8(busio+0x1b,2);for(volatile int w=0;w<10000;w++){}
    o8(busio+0x1b,0);
    o32(busio+0x10,(u32)(uintptr_t)bdl);
    o8(busio+0x15,(u8)(queued-1));lastciv=i8(busio+0x14)&31;
    o16(busio+0x16,0x001c);
    o8(busio+0x1b,1);playing=1;paused=0;err="Reproduciendo WAV";
    kprintf("[vortex-audio] WAV PCM: %u Hz %u canales %u bits\n",rate,(u32)channels,(u32)bits);
    return 0;
}
void audio_tick(void){
    if(!playing||paused||!found)return;
    int civ=i8(busio+0x14)&31;
    int walked=0;
    while(lastciv!=civ&&walked++<32){
        lastciv=(lastciv+1)&31;
        if(queued>0)queued--;
        if(srcidx<frames&&queued<NSLOT){
            int idx=fillidx;
            if(refill(idx)){fillidx=(idx+1)&31;queued++;o8(busio+0x15,(u8)idx);}
        }
    }
    if(srcidx>=frames){
        u16 sr=i16(busio+0x16);
        if(!queued||(sr&1)){audio_stop();err="Finalizado";}
    }
}
int audio_active(void){return playing;}
int audio_paused(void){return paused;}
void audio_pause(int p){
    if(!playing)return;
    paused=!!p;u8 v=i8(busio+0x1b);o8(busio+0x1b,paused?(v&~1u):(v|1u));
}
int audio_progress(void){
    if(!frames)return 0;return (int)((u64)srcidx*100u/frames);
}
const char *audio_error(void){return err;}
#endif
