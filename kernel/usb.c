/* Deteccion de controladores USB PCI, sin afirmar compatibilidad de almacenamiento. */
#include "usb.h"
#include "vortex.h"
static inline void out32(unsigned short p,unsigned v){__asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p));}
static inline unsigned in32(unsigned short p){unsigned v;__asm__ volatile("inl %1,%0":"=a"(v):"Nd"(p));return v;}
static unsigned pcir(int b,int d,int f,int off){
    out32(0xCF8,0x80000000u|((unsigned)b<<16)|((unsigned)d<<11)|((unsigned)f<<8)|((unsigned)off&0xFC));
    return in32(0xCFC);
}
static int count,first=-1;static uint32_t last,scanned;
static void scan(void){
    uint32_t t=kticks();if(scanned&&t-last<500)return;
    count=0;first=-1;
    /* Se lee la configuracion de PCI sin activar ningun controlador ni modificar hardware. */
    for(int b=0;b<256;b++)for(int d=0;d<32;d++){
        unsigned x=pcir(b,d,0,0);if((x&0xFFFFu)==0xFFFFu)continue;
        int nf=(pcir(b,d,0,0x0C)&0x00800000u)?8:1;
        for(int f=0;f<nf;f++){
            if((pcir(b,d,f,0)&0xFFFFu)==0xFFFFu)continue;
            unsigned cls=pcir(b,d,f,0x08);
            if((cls>>16)==0x0C03u){int typ=(cls>>8)&255;count++;if(first<0)first=typ;}
        }
    }
    last=t;scanned=1;
}
int usb_controllers(void){scan();return count;}
int usb_first_controller_type(void){scan();return first;}
const char *usb_controller_name(int t){
    if(t==0)return "UHCI (USB 1.1)";
    if(t==0x10)return "OHCI (USB 1.1)";
    if(t==0x20)return "EHCI (USB 2.0)";
    if(t==0x30)return "XHCI (USB 3.x)";
    return t<0?"ninguno":"controlador USB";
}
