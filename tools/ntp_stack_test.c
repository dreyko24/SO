/* Prueba la integracion NTP del codigo REAL de red (HOSTNET), sin Internet. */
#define main vortex_net_sim_main
#include "nettest_main.c"
#undef main
static int errors;
static void check(int ok,const char *msg){
    if(!ok){ fprintf(stderr,"NTP STACK FAIL: %s\n",msg); errors++; }
}
static void put32(uint8_t *p,uint32_t n){p[0]=n>>24;p[1]=n>>16;p[2]=n>>8;p[3]=n;}
int main(void){
    T=1000;
    check(!net_time_synced(),"el arranque usa reloj RTC");
    check(net_utc_epoch()==1794650400ull,"hora inicial del RTC simulado");
    ntp.phase=2; ntp.server=0xC0000201u; ntp.sent=T; ntp.tries=1;
    for(int i=0;i<8;i++) ntp.nonce[i]=(uint8_t)(i*9+3);
    uint8_t p[48]={0}; p[0]=0x24; p[1]=2;
    memcpy(p+24,ntp.nonce,8);
    put32(p+40,(uint32_t)(1794650420ull+2208988800ull));
    ntp_in(0xC0000202u,123,p,48);
    check(!ntp.valid,"rechazar IP inesperada");
    ntp_in(ntp.server,124,p,48);
    check(!ntp.valid,"rechazar puerto de origen no NTP");
    p[24]^=1; ntp_in(ntp.server,123,p,48);
    check(!ntp.valid,"rechazar respuesta sin coincidencia de solicitud");
    p[24]^=1; ntp_in(ntp.server,123,p,48);
    check(net_time_synced(),"respuesta valida activa NTP");
    check(net_utc_epoch()==1794650420ull,"UTC NTP sustituye hora RTC");
    T+=250; check(net_utc_epoch()==1794650422ull,"reloj monotono suma segundos");
    check(ntp.phase==0 && ntp.next>T,"reprograma la sincronizacion");
    if(errors) return 1;
    puts("VORTEX NTP STACK OK");
    return 0;
}
