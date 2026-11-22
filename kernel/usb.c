/* Deteccion de controladores USB PCI, sin afirmar compatibilidad de almacenamiento. */
#include "usb.h"
#include "vortex.h"
#ifdef HOSTGUI
int usb_controllers(void){return 0;}
int usb_first_controller_type(void){return -1;}
const char *usb_controller_name(int t){(void)t;return "simulado";}
#else
static inline void out32(unsigned short p,unsigned v){__asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p));}
static inline unsigned in32(unsigned short p){unsigned v;__asm__ volatile("inl %1,%0":"=a"(v):"Nd"(p));return v;}
static unsigned pcir(unsigned dev,int off){
    out32(0xCF8,0x80000000u|dev|((unsigned)off&0xFC));
    return in32(0xCFC);
}
static int count,first=-1,xhci=-1;static uint32_t last,scanned;
static void scan(void){
    uint32_t t=kticks();if(scanned&&t-last<500)return;
    count=0;first=xhci=-1;
    /* Se lee la configuracion de PCI sin activar ningun controlador ni modificar hardware. */
    for(int b=0;b<256;b++)for(int d=0;d<32;d++){
        unsigned dev=((unsigned)b<<16)|((unsigned)d<<11);
        unsigned x=pcir(dev,0);if((x&0xFFFFu)==0xFFFFu)continue;
        int nf=(pcir(dev,0x0C)&0x00800000u)?8:1;
        for(int f=0;f<nf;f++){
            unsigned func=dev|((unsigned)f<<8);
            if((pcir(func,0)&0xFFFFu)==0xFFFFu)continue;
            unsigned cls=pcir(func,0x08);
            if((cls>>16)==0x0C03u){int typ=(cls>>8)&255;count++;if(first<0)first=typ;}
            if((cls>>8)==0x0c0330u&&xhci<0)xhci=(int)func;
        }
    }
    last=t;scanned=1;
}
int usb_controllers(void){scan();return count;}
int usb_first_controller_type(void){scan();return first;}
uint64_t usb_pci_enable_xhci(void){
    scan();
    if(xhci<0)return 0;
    uint32_t lo=pcir((unsigned)xhci,0x10);
    if((lo&1u)||lo==0xffffffffu)return 0;
    uint64_t bar=lo&~15u;
    if((lo&6u)==4u)bar|=(uint64_t)pcir((unsigned)xhci,0x14)<<32;
    if(!bar)return 0;
    unsigned command=pcir((unsigned)xhci,4)&0xffffu;
    out32(0xCF8,0x80000004u|(unsigned)xhci);
    out32(0xCFC,command|6u); /* memoria MMIO y bus master; no tocar los bits W1C del estado */
    return bar;
}
const char *usb_controller_name(int t){
    if(t==0)return "UHCI (USB 1.1)";
    if(t==0x10)return "OHCI (USB 1.1)";
    if(t==0x20)return "EHCI (USB 2.0)";
    if(t==0x30)return "XHCI (USB 3.x)";
    return t<0?"ninguno":"controlador USB";
}

#endif /* HOSTGUI */
