/* xHCI nativo: enumeración de puertos raíz y de descriptores por el endpoint 0. */
#include "usb_xhci.h"
#include "usb.h"
#include "disk.h"

enum {
    TRBS = 64, MAX_SLOTS = 16, MAX_SCRATCHPADS = 32, MMIO_WINDOW = 0x10000,
    RUN = 1u << 0, RESET = 1u << 1, HALTED = 1u << 0, NOT_READY = 1u << 11,
    PORT_CONNECTED = 1u << 0, PORT_ENABLED = 1u << 1,
    PORT_RESET = 1u << 4, PORT_POWER = 1u << 9, PORT_WARM_RESET = 1u << 31,
    PORT_CONNECT_CHANGE = 1u << 17, PORT_CHANGES = 0x00fe0000u,
    PORT_RW = PORT_POWER | (3u << 14) | (7u << 25),
    TRB_CYCLE = 1u << 0, TRB_TOGGLE = 1u << 1,
    TRB_IMMEDIATE = 1u << 6, TRB_IOC = 1u << 5, TRB_IN = 1u << 16,
    TRB_SETUP = 2, TRB_DATA = 3, TRB_STATUS = 4, TRB_LINK = 6,
    TRB_ENABLE_SLOT = 9, TRB_DISABLE_SLOT = 10, TRB_ADDRESS = 11,
    TRB_EVALUATE = 13, EVENT_TRANSFER = 32, EVENT_COMMAND = 33
};
#define TRB_TYPE(type) ((uint32_t)(type) << 10)
#define TRB_GET_TYPE(flags) (((flags) >> 10) & 63u)

typedef struct {
    uint32_t lo, hi, status, flags;
} Trb;

typedef struct {
    Trb *trbs;
    unsigned index, cycle;
} Ring;

typedef struct {
    uint64_t base;
    uint32_t size, reserved;
} Segment;

typedef struct {
    uint32_t observed_at;
    uint8_t seen, tried, slot, protocol, slot_type;
} RootPort;

static uint64_t dcbaa[MAX_SLOTS + 1] __attribute__((aligned(4096)));
static uint64_t scratch_addresses[MAX_SCRATCHPADS] __attribute__((aligned(4096)));
static uint8_t scratches[MAX_SCRATCHPADS][4096] __attribute__((aligned(4096)));
static uint32_t input[1024] __attribute__((aligned(4096)));
static uint32_t output[MAX_SLOTS][1024] __attribute__((aligned(4096)));
static Trb command_trbs[TRBS] __attribute__((aligned(4096)));
static Trb control_trbs[MAX_SLOTS][TRBS] __attribute__((aligned(4096)));
static volatile Trb events[TRBS] __attribute__((aligned(4096)));
static Segment event_segment __attribute__((aligned(64)));
static uint8_t data[4096] __attribute__((aligned(4096)));
static Ring commands, controls[MAX_SLOTS];
static RootPort ports[256];
static unsigned slots, port_count, ctx_words, event_index, event_cycle = 1;
static unsigned active;
static int tried, online, present;
static volatile uint8_t *mmio, *oper, *doorbells, *interrupter;

static void barrier(void){ __asm__ volatile("" ::: "memory"); }

static uint32_t read32(volatile uint8_t *base,unsigned off){
    return *(volatile uint32_t *)(base + off);
}
static void write32(volatile uint8_t *base,unsigned off,uint32_t value){
    *(volatile uint32_t *)(base + off) = value;
}
static void write64(volatile uint8_t *base,unsigned off,uint64_t value){
#if UINTPTR_MAX > UINT32_MAX
    *(volatile uint64_t *)(base + off) = value;
#else
    write32(base,off,(uint32_t)value);
    write32(base,off+4,(uint32_t)(value>>32));
#endif
}
static int wait_mask(volatile uint8_t *base,unsigned off,uint32_t mask,uint32_t value){
    uint32_t start=kticks();
    do {
        uint32_t v=read32(base,off);
        if(v!=UINT32_MAX&&(v&mask)==value)return 0;
        __asm__ volatile("pause");
    } while(kticks()-start<100);
    return -1;
}
static int capabilities(uint32_t hcc){
    unsigned offset=(hcc>>16)*4;
    while(offset){
        if(offset>MMIO_WINDOW-16)return -1;
        uint32_t cap=read32(mmio,offset);
        if(cap==UINT32_MAX)return -1;
        if((cap&255)==1){
            write32(mmio,offset,cap|(1u<<24)); /* OS Owned Semaphore */
            if(wait_mask(mmio,offset,1u<<16,0))return -1;
            uint32_t legacy=read32(mmio,offset+4);
            /* Desactivar los SMI del firmware y reconocer los estados W1C. */
            write32(mmio,offset+4,(legacy&0x000e1feeu)|0xe0110000u);
        }else if((cap&255)==2){
            uint32_t range=read32(mmio,offset+8);
            unsigned first=range&255, count=(range>>8)&255;
            uint8_t slot_type=(uint8_t)(read32(mmio,offset+12)&31);
            for(unsigned i=first;i&&i<=port_count&&i<first+count;i++){
                ports[i].protocol=(uint8_t)(cap>>24);
                ports[i].slot_type=slot_type;
            }
        }
        unsigned next=(cap>>8)&255;
        if(!next)break;
        offset+=next*4;
    }
    return 0;
}
static void ring_init(Ring *r,Trb *trbs){
    memset(trbs,0,sizeof(Trb)*TRBS);
    *r=(Ring){trbs,0,1};
}
static Trb *push(Ring *r,uint64_t pointer,uint32_t status,uint32_t flags){
    Trb *t=&r->trbs[r->index++];
    t->lo=(uint32_t)pointer;
    t->hi=(uint32_t)(pointer>>32);
    t->status=status;
    barrier();
    t->flags=flags|r->cycle;
    if(r->index==TRBS-1){
        Trb *link=&r->trbs[TRBS-1];
        uint64_t base=(uintptr_t)r->trbs;
        link->lo=(uint32_t)base;
        link->hi=(uint32_t)(base>>32);
        link->status=0;
        barrier();
        link->flags=TRB_TYPE(TRB_LINK)|TRB_TOGGLE|r->cycle;
        r->index=0;
        r->cycle^=1;
    }
    return t;
}
static int pop(Trb *t){
    volatile Trb *e=&events[event_index];
    uint32_t flags=e->flags;
    if((flags&TRB_CYCLE)!=event_cycle)return 0;
    barrier();
    *t=(Trb){e->lo,e->hi,e->status,flags};
    if(++event_index==TRBS){event_index=0;event_cycle^=1;}
    write64(interrupter,0x18,(uintptr_t)&events[event_index]|8u);
    return 1;
}
static int initialize(uint64_t address){
    if(address>UINTPTR_MAX-MMIO_WINDOW)return -1;
#if UINTPTR_MAX > UINT32_MAX
    if(address+MMIO_WINDOW>(16ull<<30)&&
       (!vortex_mmio_map||!vortex_mmio_map(address,MMIO_WINDOW)))return -1;
#endif
    mmio=(volatile uint8_t *)(uintptr_t)address;
    unsigned op=read32(mmio,0)&255;
    uint32_t hcs1=read32(mmio,4),hcs2=read32(mmio,8),hcc=read32(mmio,0x10);
    unsigned db=read32(mmio,0x14)&~3u,rt=read32(mmio,0x18)&~31u;
    unsigned scratch=(((hcs2>>21)&31)<<5)|((hcs2>>27)&31);
    slots=hcs1&255;
    if(slots>MAX_SLOTS)slots=MAX_SLOTS;
    port_count=hcs1>>24;
    ctx_words=(hcc&4u)?16:8;
    if(op<0x20||(op&3)||!slots||!port_count||db<op||db>MMIO_WINDOW-4*(slots+1)||
       rt<op||rt>MMIO_WINDOW-0x40||scratch>MAX_SCRATCHPADS)return -1;
    oper=mmio+op;
    doorbells=mmio+db;
    interrupter=mmio+rt+0x20;
    if(capabilities(hcc)||wait_mask(oper,4,NOT_READY,0))return -1;
    write32(oper,0,0);
    if(wait_mask(oper,4,HALTED,HALTED))return -1;
    write32(oper,0,RESET);
    if(wait_mask(oper,0,RESET,0)||wait_mask(oper,4,NOT_READY,0)||
       !(read32(oper,8)&1u))return -1; /* PAGESIZE: 4 KiB */
    for(unsigned i=0;i<scratch;i++)scratch_addresses[i]=(uintptr_t)scratches[i];
    if(scratch)dcbaa[0]=(uintptr_t)scratch_addresses;
    ring_init(&commands,command_trbs);
    event_segment=(Segment){(uintptr_t)events,TRBS,0};
    barrier();
    write64(oper,0x30,(uintptr_t)dcbaa);
    write64(oper,0x18,(uintptr_t)command_trbs|1u);
    write32(oper,0x38,slots);
    write32(interrupter,0,1); /* sin interrupciones; consumir eventos por sondeo */
    write32(interrupter,8,1);
    write64(interrupter,0x18,(uintptr_t)events);
    write64(interrupter,0x10,(uintptr_t)&event_segment);
    write32(oper,0,RUN);
    if(!wait_mask(oper,4,HALTED,0))return 0;
    write32(oper,0,0);
    return -1;
}
static int completion(unsigned type,Trb *trb,unsigned slot,Trb *result){
    uint32_t start=kticks();
    do {
        Trb event;
        if(!pop(&event)){__asm__ volatile("pause");continue;}
        if(TRB_GET_TYPE(event.flags)!=type||
           (slot&&(event.flags>>24!=slot||((event.flags>>16)&31)!=1)))continue;
        uint64_t ptr=((uint64_t)event.hi<<32)|event.lo;
        unsigned cc=event.status>>24;
        if(ptr==(uintptr_t)trb){
            *result=event;
            return cc==1?0:-(int)cc;
        }
        if(type==EVENT_TRANSFER&&cc!=1)return -(int)cc;
    } while(kticks()-start<100);
    /* Una operación todavía en vuelo podría tocar los mismos buffers. */
    online=0;
    write32(oper,0,0);
    kprintf("[vortex-xhci] sin evento de finalizacion tipo=%u slot=%u\n",type,slot);
    return -1;
}
static int command(uint64_t pointer,uint32_t flags,unsigned *slot){
    Trb *pending=push(&commands,pointer,0,flags);
    Trb event;
    barrier();
    write32(doorbells,0,0);
    int rc=completion(EVENT_COMMAND,pending,0,&event);
    if(!rc&&slot)*slot=event.flags>>24;
    return rc;
}
static int ep0_get(unsigned slot,unsigned descriptor,unsigned bytes){
    Ring *r=&controls[slot-1];
    uint64_t setup=0x0680u|((uint64_t)descriptor<<24)|((uint64_t)bytes<<48);
    memset(data,0,bytes);
    push(r,setup,8,TRB_TYPE(TRB_SETUP)|TRB_IMMEDIATE|(3u<<16));
    push(r,(uintptr_t)data,bytes,TRB_TYPE(TRB_DATA)|TRB_IN);
    Trb *pending=push(r,0,0,TRB_TYPE(TRB_STATUS)|TRB_IOC);
    Trb event;
    barrier();
    write32(doorbells,slot*4,1);
    return completion(EVENT_TRANSFER,pending,slot,&event);
}
static void endpoint_context(unsigned slot,unsigned packet){
    uint32_t *ep=input+2*ctx_words;
    uint64_t ring=(uintptr_t)control_trbs[slot-1]|1u;
    ep[1]=(packet<<16)|(4u<<3)|(3u<<1);
    ep[2]=(uint32_t)ring;
    ep[3]=(uint32_t)(ring>>32);
    ep[4]=8;
}
static int address_slot(unsigned port,unsigned slot,unsigned speed,unsigned packet){
    memset(input,0,sizeof input);
    memset(output[slot-1],0,sizeof output[0]);
    ring_init(&controls[slot-1],control_trbs[slot-1]);
    dcbaa[slot]=(uintptr_t)output[slot-1];
    input[1]=3;
    uint32_t *ctx=input+ctx_words;
    ctx[0]=(1u<<27)|(speed<<20); /* un endpoint, ruta 0 para un dispositivo raíz */
    ctx[1]=port<<16;
    endpoint_context(slot,packet);
    return command((uintptr_t)input,TRB_TYPE(TRB_ADDRESS)|(slot<<24),0);
}
static int read_identity(unsigned slot,int superspeed,unsigned packet,
                         unsigned *vid,unsigned *pid,unsigned *klass){
    if(ep0_get(slot,1,8)||data[0]<18||data[1]!=1)return -1;
    unsigned max_packet=superspeed?(data[7]==9?512:0):data[7];
    if(max_packet!=packet){
        if(packet!=8||(max_packet!=16&&max_packet!=32&&max_packet!=64))return -1;
        memset(input,0,sizeof input);
        input[1]=2;
        input[2*ctx_words+1]=max_packet<<16;
        if(command((uintptr_t)input,TRB_TYPE(TRB_EVALUATE)|(slot<<24),0))return -1;
    }
    if(ep0_get(slot,1,18)||data[0]<18||data[1]!=1)return -1;
    *vid=data[8]|((unsigned)data[9]<<8);
    *pid=data[10]|((unsigned)data[11]<<8);
    *klass=data[4];
    if(*klass&&*klass!=0xef)return 0;
    if(ep0_get(slot,2,9)||data[0]!=9||data[1]!=2)return -1;
    unsigned size=data[2]|((unsigned)data[3]<<8);
    if(size<9||size>sizeof data||ep0_get(slot,2,size)||data[0]!=9||data[1]!=2)return -1;
    for(unsigned off=0;off+2<=size;off+=data[off]){
        unsigned len=data[off];
        if(len<2||len>size-off)return -1;
        if(data[off+1]==4&&len>=9&&!data[off+3]){
            *klass=data[off+5];
            return 0;
        }
    }
    return -1;
}
static unsigned portsc(unsigned port){return 0x400+16*(port-1);}

static int is_superspeed(unsigned port,unsigned speed){
    unsigned protocol=ports[port].protocol;
    return protocol?protocol>=3:speed>=4;
}
static int port_speed(unsigned port,uint32_t state){
    unsigned off=portsc(port);
    if(!(state&PORT_POWER)){
        write32(oper,off,(state&PORT_RW)|PORT_POWER);
        if(wait_mask(oper,off,PORT_POWER,PORT_POWER))return 0;
        state=read32(oper,off);
    }
    unsigned speed=(state>>10)&15;
    if(!is_superspeed(port,speed)){
        write32(oper,off,(state&PORT_RW)|PORT_RESET);
        if(wait_mask(oper,off,PORT_RESET,0))return 0;
        state=read32(oper,off);
    }else if(!(state&PORT_ENABLED)){
        write32(oper,off,(state&PORT_RW)|PORT_WARM_RESET);
        if(wait_mask(oper,off,PORT_ENABLED,PORT_ENABLED))return 0;
        state=read32(oper,off);
    }
    if((state&(PORT_CONNECTED|PORT_ENABLED))!=(PORT_CONNECTED|PORT_ENABLED))return 0;
    write32(oper,off,state&(PORT_RW|PORT_CHANGES));
    return (state>>10)&15;
}
static void disable_slot(unsigned slot){
    if(!online||command(0,TRB_TYPE(TRB_DISABLE_SLOT)|(slot<<24),0))return;
    dcbaa[slot]=0;
}
static void enumerate(unsigned port,uint32_t state){
    unsigned speed=(unsigned)port_speed(port,state);
    if(!speed)return;
    unsigned slot;
    uint32_t flags=TRB_TYPE(TRB_ENABLE_SLOT)|((uint32_t)ports[port].slot_type<<16);
    if(command(0,flags,&slot)||!slot||slot>slots)return;
    int superspeed=is_superspeed(port,speed);
    unsigned packet=superspeed?512:speed==3?64:8;
    unsigned vid,pid,klass;
    if(address_slot(port,slot,speed,packet)||
       read_identity(slot,superspeed,packet,&vid,&pid,&klass)){
        kprintf("[vortex-xhci] error al enumerar puerto=%u velocidad=%u\n",port,speed);
        disable_slot(slot);
        return;
    }
    ports[port].slot=(uint8_t)slot;
    active++;
    kprintf("[vortex-xhci] VID=%x PID=%x clase=%x puerto=%u velocidad=%u\n",
            vid,pid,klass,port,speed);
}
void usb_xhci_poll(void){
    if(!tried){
        tried=1;
        uint64_t bar=usb_pci_enable_xhci();
        if(!bar)return;
        present=1;
        if(initialize(bar)){kprintf("[vortex-xhci] no se pudo iniciar\n");return;}
        online=1;
        kprintf("[vortex-xhci] iniciado con %u puertos raiz\n",port_count);
    }
    if(!online)return;
    Trb event;
    for(unsigned i=0;i<TRBS&&pop(&event);i++){}
    uint32_t now=kticks();
    unsigned budget=2;
    for(unsigned port=1;port<=port_count&&online;port++){
        RootPort *p=&ports[port];
        unsigned off=portsc(port);
        uint32_t state=read32(oper,off);
        if(state==UINT32_MAX)continue;
        if(!(state&PORT_CONNECTED)||(state&PORT_CONNECT_CHANGE)){
            if(p->slot){
                disable_slot(p->slot);
                p->slot=0;
                active--;
                kprintf("[vortex-xhci] desconectado puerto=%u\n",port);
            }
            p->seen=p->tried=0;
        }
        if(state&PORT_CHANGES)write32(oper,off,state&(PORT_RW|PORT_CHANGES));
        if(!(state&PORT_CONNECTED))continue;
        if(!p->seen){p->seen=1;p->observed_at=now;}
        if(!p->tried&&budget&&now-p->observed_at>=10){
            p->tried=1;
            budget--;
            enumerate(port,state);
        }
    }
}
const char *usb_xhci_message(void){
    if(!present)return 0;
    if(!online)return "xHCI: controladora no disponible";
    if(active)return "xHCI: periferico reconocido por descriptor. Unidad de archivos aun no disponible";
    return "xHCI: sin perifericos reconocidos en puertos raiz";
}
