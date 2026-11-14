#ifndef VORTEX_PS2_MOUSE_H
#define VORTEX_PS2_MOUSE_H
#include <stdint.h>

/* IntelliMouse PS/2: ID 0 usa 3 bytes; ID 3/4 añade rueda en el cuarto.
 * Decodificador sin acceso a puertos; se prueba en el anfitrión. */
typedef struct { int dx,dy,buttons,wheel; } VMouseEvent;
typedef struct {
    uint8_t bytes[4];
    int n,packet_size;
} VMouseDecoder;
static void vmouse_init(VMouseDecoder *m,int device_id){
    m->n=0;m->packet_size=(device_id==3||device_id==4)?4:3;
}
static int vmouse_feed(VMouseDecoder *m,uint8_t data,VMouseEvent *out){
    if(m->n==0&&!(data&8))return 0; /* sincronizar comienzo */
    m->bytes[m->n++]=data;
    if(m->n<m->packet_size)return 0;
    m->n=0;
    if(m->bytes[0]&0xc0)return 0; /* overflow X/Y */
    int dx=m->bytes[1],dy=m->bytes[2];
    if(m->bytes[0]&0x10)dx-=256;
    if(m->bytes[0]&0x20)dy-=256;
    out->dx=dx;out->dy=-dy;out->buttons=m->bytes[0]&7;
    /* Rueda: nibble con signo de 4 bits; no tratar botones 4/5 como rueda. */
    int wheel=m->packet_size==4?(m->bytes[3]&15):0;
    out->wheel=wheel&8?wheel-16:wheel;
    return 1;
}
#endif
