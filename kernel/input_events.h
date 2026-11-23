#ifndef VORTEX_INPUT_EVENTS_H
#define VORTEX_INPUT_EVENTS_H

#include <stdint.h>

/* Llamar con las interrupciones protegidas si comparte productor PS/2. */
enum { INPUT_PS2=1, INPUT_USB=2 };
typedef struct {
    uint8_t held[256];
} VKeySources;

static inline unsigned vkey_index(uint8_t raw,int extended){
    return (raw&0x7f)|(extended?0x80:0);
}
static inline int vkey_source(VKeySources *keys,uint8_t raw,int extended,unsigned source){
    unsigned index=vkey_index(raw,extended);
    uint8_t before=keys->held[index];
    if(raw&0x80){
        keys->held[index]=before&~source;
        return (before&source)&&!keys->held[index];
    }
    keys->held[index]=before|source;
    return !before||(before&source); /* La misma fuente puede repetir la pulsación. */
}

#define VKEY_RAW_SIZE 256
typedef struct {
    uint16_t events[VKEY_RAW_SIZE];
    uint8_t delivered[256];
    unsigned head, tail, reconcile;
    int overflow;
} VRawKeyQueue;

static inline void vkey_raw_push(VRawKeyQueue *queue,uint8_t raw,int extended){
    unsigned next=(queue->head+1)%VKEY_RAW_SIZE;
    if(next==queue->tail){
        queue->overflow=1;
        queue->reconcile=0;
        return;
    }
    queue->events[queue->head]=raw|(extended?0x100:0);
    queue->head=next;
}
static inline int vkey_raw_pop(VRawKeyQueue *queue,const VKeySources *keys){
    if(queue->tail!=queue->head){
        int key=queue->events[queue->tail];
        queue->tail=(queue->tail+1)%VKEY_RAW_SIZE;
        queue->delivered[vkey_index(key,key&0x100)]=!(key&0x80);
        return key;
    }
    if(queue->overflow){
        /* Si se perdió una liberación, reconciliar después de los eventos en cola. */
        while(queue->reconcile<sizeof keys->held){
            unsigned index=queue->reconcile++;
            int held=!!keys->held[index];
            if(held==queue->delivered[index])continue;
            queue->delivered[index]=held;
            return (index&0x7f)|(held?0:0x80)|((index&0x80)?0x100:0);
        }
        queue->overflow=0;
    }
    return -1;
}

#define VMOUSE_QUEUE_SIZE 64
typedef struct { int dx,dy,buttons,wheel; } VMouseEvent;
typedef struct {
    VMouseEvent events[VMOUSE_QUEUE_SIZE];
    unsigned head, tail;
    int buttons, delivered_buttons;
} VMouseQueue;

static inline void vmouse_push(VMouseQueue *queue,VMouseEvent event){
    unsigned next=(queue->head+1)%VMOUSE_QUEUE_SIZE;
    queue->buttons=event.buttons;
    if(next==queue->tail)return;
    queue->events[queue->head]=event;
    queue->head=next;
}
static inline int vmouse_pop(VMouseQueue *queue,VMouseEvent *event){
    if(queue->tail!=queue->head){
        *event=queue->events[queue->tail];
        queue->tail=(queue->tail+1)%VMOUSE_QUEUE_SIZE;
    }else{
        /* Un movimiento perdido es tolerable; una liberación atascada no. */
        if(queue->buttons==queue->delivered_buttons)return 0;
        *event=(VMouseEvent){.buttons=queue->buttons};
    }
    queue->delivered_buttons=event->buttons;
    return 1;
}

#endif
