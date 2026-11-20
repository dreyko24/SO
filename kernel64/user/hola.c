/* hola.c — programa de prueba SIN libc: usa solo llamadas al sistema (ABI x86-64 de Linux) */
typedef unsigned long u64;
static inline long sc(long n,long a,long b,long c){ long r; __asm__ volatile("syscall":"=a"(r):"a"(n),"D"(a),"S"(b),"d"(c):"rcx","r11","memory"); return r; }
static void say(const char *s){ long n=0; while(s[n]) n++; sc(1,1,(long)s,n); }
static void num(long v){ char b[24]; int i=23; b[i]=0; int neg=v<0; if(neg) v=-v; do{ b[--i]='0'+v%10; v/=10; }while(v); if(neg) b[--i]='-'; say(b+i); }
struct iov { const void *b; u64 n; };
void _start(void){
    say("hola desde ring 3: ELF64 propio, llamadas al sistema estilo Linux x86-64\n");
    say("pid="); num(sc(39,0,0,0)); say("\n");
    long b0=sc(12,0,0,0); long b1=sc(12,b0+8192,0,0);
    if(b1==b0+8192){ char *p=(char*)b0; for(int i=0;i<8192;i++) p[i]=(char)i; long s=0; for(int i=0;i<8192;i++) s+=(unsigned char)p[i]; say("brk ok, suma="); num(s); say("\n"); } else say("brk FALLO\n");
    long m=sc(9,0,16384,3|(0x22<<0)); /* mmap(NULL,16K,RW,...) se arma abajo */
    { long r; register long r10 __asm__("r10")=0x22; register long r8 __asm__("r8")=-1; register long r9 __asm__("r9")=0;
      __asm__ volatile("syscall":"=a"(r):"a"(9),"D"(0),"S"(16384),"d"(3),"r"(r10),"r"(r8),"r"(r9):"rcx","r11","memory"); m=r; }
    if(m>0){ int *q=(int*)m; q[0]=1234; q[4000]=5678; say("mmap ok, valores="); num(q[0]+q[4000]); say("\n"); } else { say("mmap FALLO "); num(m); say("\n"); }
    struct iov v[2]={{"writev ",7},{"ok\n",3}}; sc(20,1,(long)v,2);
    char un[390]; for(int i=0;i<390;i++) un[i]=0; sc(63,(long)un,0,0); say("uname: "); say(un); say(" "); say(un+130); say("\n");
    /* ARCH_SET_GS=0x1001 y ARCH_GET_GS=0x1004: Firefox usa GS
     * para su runtime Wasm. Verificar MSR real, no solo el retorno 0. */
    static u64 gs_value=0x564f525445584753ul;
    u64 original_gs=0, observed_gs=0, gs_read=0;
    long gs_ok=sc(158,0x1004,(long)&original_gs,0)==0;
    if(gs_ok) gs_ok=sc(158,0x1001,(long)&gs_value,0)==0;
    if(gs_ok) gs_ok=sc(158,0x1004,(long)&observed_gs,0)==0;
    if(gs_ok){
        __asm__ volatile("movq %%gs:0,%0":"=r"(gs_read));
        gs_ok=observed_gs==(u64)&gs_value && gs_read==gs_value;
    }
    if(gs_ok) gs_ok=sc(158,0x1001,-1,0)==-22;  /* nunca WRMSR no canonico */
    sc(158,0x1001,(long)original_gs,0);
    say(gs_ok?"arch_prctl GS ok\\n":"arch_prctl GS FALLO\\n");
    sc(60,42,0,0);
    for(;;);
}
