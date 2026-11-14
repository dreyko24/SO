#include <assert.h>
#include <stdio.h>
#include "../kernel/ps2_mouse.h"
static int ev(VMouseDecoder *m,const unsigned char *a,int n,VMouseEvent *e){
    int ret=0;for(int i=0;i<n;i++)ret+=vmouse_feed(m,a[i],e);return ret;
}
int main(void){
    VMouseDecoder m;VMouseEvent e={0};
    vmouse_init(&m,3);
    const unsigned char d1[]={0x08,0x00,0x00,0x0f};
    assert(ev(&m,d1,4,&e)==1&&e.wheel==-1&&e.dx==0);
    const unsigned char d2[]={0x08,0x00,0x00,0x01};
    assert(ev(&m,d2,4,&e)==1&&e.wheel==1);
    const unsigned char d3[]={0x38,0xfd,0xfe,0x00};
    assert(ev(&m,d3,4,&e)==1&&e.dx==-3&&e.dy==2);
    const unsigned char d4[]={0x48,0,0,0};
    assert(ev(&m,d4,4,&e)==0);  /* paquete desbordado */
    vmouse_init(&m,0);
    const unsigned char d5[]={0x08,1,0};
    assert(ev(&m,d5,3,&e)==1&&e.dx==1&&e.wheel==0);
    puts("VORTEX PS2 MOUSE: 3/4 BYTES, RUEDA, SIGNO Y OVERFLOW OK");
    return 0;
}
