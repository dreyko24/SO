/* hostgui.c — simulador del escritorio en el host: compila el MISMO codigo de la interfaz (gfx, fs, html, gui),
 * simula teclado/raton y guarda capturas PPM. Uso: tools/hostgui.py (convierte a PNG). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include "../kernel/vortex.h"
int kattach_pid;
char kshell_cwd[96];
void (*kproc_done)(int,int);
void kserial_puts(const char *s){fputs(s,stderr);}
void kinput_event(int t,int c,int v){(void)t;(void)c;(void)v;}
void kproc_key(int c){(void)c;}
static int host_alloc_budget=-1;
void *kmalloc(size_t n){
    if(host_alloc_budget==0)return NULL;
    if(host_alloc_budget>0)host_alloc_budget--;
    return malloc(n);
}
void kfree(void *p){ free(p); }
static uint32_t T=1000;
uint32_t kticks(void){ return T; }
int kgetc(void){ return -1; }
void (*kcon_hook)(char);
static void out(const char *s){ while(*s){ if(kcon_hook) kcon_hook(*s); s++; } }
void kprintf(const char *f,...){ (void)f; }
void kshell_exec(char *l){
    if(!strcmp(l,"info")) out("vortex-kernel 0.5 (i386, Multiboot)\nParte de Vortex OS.\n");
    else if(!strcmp(l,"uptime")) out("12.3 s\n");
    else { out("comando desconocido: "); out(l); out(" (prueba 'help')\n"); }
}
void kreboot(void){ puts("[reboot]"); }
void khalt(void){ puts("[halt]"); }
int krtc(int *h,int *m){ *h=15; *m=45; return 0; }   /* 15:45 UTC -> 10:45 en UTC-5 */
uint32_t kram_kb(void){ return 262144; }
/* ---- red simulada para la interfaz: http_get "descarga" tras unos cuadros ---- */
#include "../kernel/net.h"
static int fk_state, fk_wait; static char fk_url[300], fk_err[100];
static const char FK_PAGE[]="<html><head><title>Pagina de prueba</title><style>body{color:red}</style><script>alert('no se debe ver')</script></head>"
 "<body><!-- comentario --><h1>example.test</h1><p>Esta pagina llego por <b>HTTP</b> &mdash; con entidades: &copy; &amp; &lt;etiqueta&gt; &#233; &eacute;.</p>"
 "<h2>Enlaces</h2><ul><li><a href=\"/otra\">Ruta absoluta</a></li><li><a href=\"sub/pagina.html\">Ruta relativa</a></li><li><a href=\"../arriba\">Padre</a></li>"
 "<li><a href=\"http://otro.test:8080/x?y=1\">Otro sitio</a></li><li><a href=\"#ancla\">Ancla</a></li></ul>"
 "<table><tr><td>uno</td><td>dos</td></tr></table><h3>Titulo tres</h3><p><i>enfasis</i> y <strong>fuerte</strong>.</p><img src=\"x.png\" alt=\"logo\"></body></html>";
int http_get(const char *url){ k_strcpy(fk_url,url,sizeof fk_url); fk_err[0]=0;
    if(k_startswith(url,"https://")){ k_strcpy(fk_err,"HTTPS no esta soportado todavia (vortex no tiene TLS). Usa una direccion http://",sizeof fk_err); return -1; }
    fk_state=0; fk_wait=3; return 0; }
int http_poll(void){ if(fk_state) return fk_state; if(--fk_wait>0) return 0; if(strstr(fk_url,"fallo")){ k_strcpy(fk_err,"No se pudo resolver el nombre: fallo.test",sizeof fk_err); fk_state=-1; } else fk_state=1; return fk_state; }
char *http_take(int *len){ char *b=malloc(sizeof FK_PAGE); memcpy(b,FK_PAGE,sizeof FK_PAGE); *len=sizeof FK_PAGE-1; return b; }
int http_is_html(void){ return 1; }
int http_status(void){ return 200; }
const char *http_error(void){ return fk_err; }
const char *http_final_url(void){ return fk_url; }
void http_cancel(void){ fk_state=0; fk_wait=0; }
void net_info(char *o,int max){ k_strcpy(o,"Red: Intel e1000  IP 192.168.7.50",max); }
int net_state(void){ return 2; }
#include "../kernel/gfx.c"
#include "../kernel/fs.c"
#define HOSTDISK 1
#include "../kernel/disk.c"
#include "../kernel/html.c"
#include "../kernel/kbd.c"
#include "../kernel/gui.c"

#define W 1024
#define H 768
static uint32_t *fb;
static void shot(const char *name){
    gui_frame();
    char p[128]; snprintf(p,sizeof p,"%s/%s.ppm",getenv("OUTDIR")?getenv("OUTDIR"):"/tmp",name);
    FILE *f=fopen(p,"wb"); fprintf(f,"P6\n%d %d\n255\n",W,H);
    for(int i=0;i<W*H;i++){ uint32_t c=fb[i]; fputc((c>>16)&255,f); fputc((c>>8)&255,f); fputc(c&255,f); }
    fclose(f);
}
static void click(int x,int y){ gui_mouse_abs(x,y,0); gui_frame(); gui_mouse_abs(x,y,1); gui_frame(); gui_mouse_abs(x,y,0); gui_frame(); T+=5; }
static void dclick(int x,int y){ click(x,y); T+=3; click(x,y); }
static void typestr(const char *s){ for(;*s;s++){ gui_key(*s=='\\'?'\n':*s); } }
static void dock(int i){int bx,by,bw,bh;dock_geom(&bx,&by,&bw,&bh);click(bx+bw/2,by+12+i*DASH_STEP+TILE/2);}
static void closeall(void){ for(int k=0;k<14;k++){ int top=win_top(); if(top<0) break; Win *w=&wins[top]; click(w->x+w->w-20,w->y+TB/2); } }
int main(void){
    fb=calloc(W*H,4);
    if(gui_init(fb,W,H,W*4)){ puts("gui_init fallo"); return 1; }
    { FsNode *m=fs_resolve(fs_root(),"/Musica");FsNode *f=m?fs_find(m,"Prueba-de-audio-16bits.wav"):0;
      if(!f||f->size!=88244||memcmp(f->data,"RIFF",4)||memcmp(f->data+8,"WAVE",4)){
          puts("FALLO archivo WAV PCM 16-bit de prueba");return 1;
      }
      puts("VORTEX ARCHIVO WAV 16-BIT DE PRUEBA OK");
    }
    /* Multitarea por teclado: ciclo completo, retroceso, y estado
     * intacto al volver a una ventana minimizada. */
    {
      closeall();
      Win *a=open_app(A_BROWSER),*b=open_app(A_FILES),*c=open_app(A_NOTES);
      if(!a||!b||!c){puts("FALLO crear ventanas Alt+Tab");return 1;}
      int ai=(int)(a-wins),bi=(int)(b-wins),ci=(int)(c-wins);
      if(win_top()!=ci||nord!=3){puts("FALLO foco inicial");return 1;}
      kbd_feed(0x38);
      gui_key(kbd_feed(0x0f));kbd_feed(0x8f);
      if(win_top()!=bi||!switch_active){puts("FALLO Alt+Tab primera");return 1;}
      gui_key(kbd_feed(0x0f));kbd_feed(0x8f);
      if(win_top()!=ai){puts("FALLO Alt+Tab tercera ventana");return 1;}
      kbd_feed(0x2a);gui_key(kbd_feed(0x0f));kbd_feed(0x8f);kbd_feed(0xaa);
      if(win_top()!=bi){puts("FALLO Mayus+Alt+Tab");return 1;}
      shot("33_conmutador_alt_tab");
      gui_key(kbd_feed(0xb8));
      if(switch_active||win_top()!=bi){puts("FALLO soltar Alt");return 1;}
      win_minimize(b);
      if(!b->minimized||win_top()==bi){puts("FALLO minimizar ventana de prueba");return 1;}
      kbd_feed(0x38);
      for(int i=0;i<3&&win_top()!=bi;i++){gui_key(kbd_feed(0x0f));kbd_feed(0x8f);}
      gui_key(kbd_feed(0xb8));
      if(win_top()!=bi||b->minimized){puts("FALLO restaurar por Alt+Tab");return 1;}
      closeall();
      puts("VORTEX MULTITAREA ALT+TAB, MAYUS Y RESTAURACION OK");
    }
    /* Redimensionar por esquina, borde izquierdo y superior sin corromper
     * contenido, sin salirse de la pantalla y sin perder el foco. */
    {
      closeall();
      Win *w=open_app(A_FILES);if(!w){puts("FALLO ventana resize");return 1;}
      int w0=w->w,h0=w->h;
      int mx0=w->x+w->w-4,my0=w->y+w->h-4;
      gui_mouse_abs(mx0,my0,0);gui_frame();
      gui_mouse_abs(mx0,my0,1);gui_frame();
      gui_mouse_abs(mx0+75,my0+60,1);gui_frame();
      gui_mouse_abs(mx0+75,my0+60,0);gui_frame();
      if(w->w!=w0+75||w->h!=h0+60||resize_win!=-1){puts("FALLO resize esquina");return 1;}
      shot("34_redimensionar_ventana");
      mx0=w->x+4;my0=w->y+w->h/2;
      gui_mouse_abs(mx0,my0,0);gui_frame();
      gui_mouse_abs(mx0,my0,1);gui_frame();
      gui_mouse_abs(mx0+40,my0,1);gui_frame();
      gui_mouse_abs(mx0+40,my0,0);gui_frame();
      if(w->x!=160||w->w!=w0+35){puts("FALLO resize borde izquierdo");return 1;}
      mx0=w->x+w->w/2;my0=w->y+3;int old_y=w->y,old_h=w->h;
      gui_mouse_abs(mx0,my0,0);gui_frame();
      gui_mouse_abs(mx0,my0,1);gui_frame();
      gui_mouse_abs(mx0,my0+20,1);gui_frame();
      gui_mouse_abs(mx0,my0+20,0);gui_frame();
      if(w->y!=old_y+20||w->h!=old_h-20){puts("FALLO resize borde superior");return 1;}
      mx0=w->x+w->w-4;my0=w->y+w->h-4;
      gui_mouse_abs(mx0,my0,0);gui_frame();
      gui_mouse_abs(mx0,my0,1);gui_frame();
      gui_mouse_abs(0,0,1);gui_frame();
      gui_mouse_abs(0,0,0);gui_frame();
      int minw,minh;win_min_dimensions(w,&minw,&minh);
      if(w->w<minw||w->h<minh||w->x<0||w->y<BARH||w->x+w->w>W||w->y+w->h>H){
          puts("FALLO resize limites de pantalla");return 1;
      }
      closeall();
      puts("VORTEX REDIMENSIONAMIENTO BORDES/ESQUINAS Y LIMITES OK");
    }
    /* Apariencia y traduccion: probar cambios persistidos y menu superior. */
    dock(7);
    { Win *v=&wins[win_top()];
      click(v->x+1+12+4*100+48,v->y+TB+10+14);
      if(set_tab!=4){puts("FALLO pestana apariencia");return 1;}
      click(v->x+1+20+94,v->y+TB+50+88+17);
      if(!light_theme){puts("FALLO modo claro");return 1;}
      { char z[20];if(!cfg_get("tema",z,sizeof z)||strcmp(z,"claro")){puts("FALLO guardar tema");return 1;} }
      shot("29_modo_claro");
      click(v->x+1+245+94,v->y+TB+50+88+17);
      if(light_theme){puts("FALLO modo oscuro");return 1;}
      shot("30_modo_oscuro");
      click(v->x+1+174+70,v->y+TB+50+208+17);
      if(ui_lang!=1||strcmp(vortex_ui_translate("Archivos",ui_lang),"Files")){puts("FALLO ingles");return 1;}
      shot("31_interfaz_ingles");
      click(v->x+1+328+70,v->y+TB+50+208+17);
      if(ui_lang!=2||strcmp(vortex_ui_translate("Archivos",ui_lang),"Willaykipukuna")){puts("FALLO quechua");return 1;}
      shot("32_interfaz_quechua");
      click(W-80,14);
      if(!language_menu_open){puts("FALLO menu idioma superior");return 1;}
      click(W-180+40,BARH+6+31+16);
      if(ui_lang!=0||language_menu_open){puts("FALLO menu idioma espanol");return 1;}
      { char z[20];if(!cfg_get("idioma",z,sizeof z)||strcmp(z,"es")){puts("FALLO guardar idioma");return 1;} }
      click(v->x+1+12+48,v->y+TB+10+14);
      if(set_tab!=0){puts("FALLO regreso a ajustes generales");return 1;}
      puts("VORTEX TEMA CLARO/OSCURO E IDIOMAS ES/EN/QU OK");
    }
    closeall();
    shot("01_escritorio");
    dock(2); T+=100;                       /* Archivos */
    dclick(170,160); shot("02_archivos");
    closeall();
    dock(2); shot("02_archivos_raiz");
    dclick(170,150);                        /* Documentos (primera fila) */
    shot("02c_archivos_documentos");
    closeall();
    dock(3); typestr("help\\ls /\\cd /Documentos\\ls\\cat notas.txt\\write hola.txt Hola desde vortex\\cat hola.txt\\info\\"); shot("03_terminal");
    closeall();
    dock(1); shot("04_navegador");
    click(190,254); shot("05_navegador_enlace");
    /* navegador con http simulado: cargando -> pagina -> enlaces relativos */
    /* Minimizar y restaurar no destruye contenido ni dimensiones. */
    closeall();dock(2);
    { int wi=win_top();Win *w=&wins[wi];
      int oldx=w->x,oldy=w->y,oldw=w->w,oldh=w->h;
      click(w->x+w->w-82,w->y+14);
      if(win_top()!=-1||!w->minimized){puts("FALLO minimizar");return 1;}
      click(322,14); /* barra de ventanas minimizadas tras espacios */
      if(win_top()!=wi||w->minimized){puts("FALLO restaurar minimizada");return 1;}
      click(w->x+w->w-51,w->y+14);
      if(!w->maxed||w->x!=0||w->y!=BARH){puts("FALLO maximizar");return 1;}
      click(w->x+w->w-51,w->y+14);
      if(w->maxed||w->x!=oldx||w->y!=oldy||w->w!=oldw||w->h!=oldh){puts("FALLO restaurar tamano");return 1;}
      puts("VORTEX VENTANAS MINIMIZAR/MAXIMIZAR OK");
    }
    closeall(); dock(1);
    { Win *b=&wins[win_top()]; click(b->x+200,b->y+TB+18); typestr("example.test/dir/index.html\\"); }
    shot("05b_http_cargando");
    gui_frame(); gui_frame(); gui_frame(); gui_frame(); shot("05c_http_pagina");
    { Win *b=&wins[win_top()];
      char u[192];
      resolve("http://example.test/dir/index.html","/otra",u,sizeof u);               if(strcmp(u,"http://example.test/otra")) { puts("FALLO resolve 1"); return 1; }
      resolve("http://example.test/dir/index.html","sub/pagina.html",u,sizeof u);     if(strcmp(u,"http://example.test/dir/sub/pagina.html")) { puts(u); puts("FALLO resolve 2"); return 1; }
      resolve("http://example.test/dir/index.html","../arriba",u,sizeof u);            if(strcmp(u,"http://example.test/arriba")) { puts(u); puts("FALLO resolve 3"); return 1; }
      resolve("http://example.test:8080/a/b?q=1","?z=2",u,sizeof u);                  if(strcmp(u,"http://example.test:8080/a/b?z=2")) { puts(u); puts("FALLO resolve 4"); return 1; }
      resolve("http://example.test/a/","//cdn.test/x",u,sizeof u);                    if(strcmp(u,"http://cdn.test/x")) { puts(u); puts("FALLO resolve 5"); return 1; }
      resolve("http://example.test/a/b","http://z.test/",u,sizeof u);                 if(strcmp(u,"http://z.test/")) { puts("FALLO resolve 6"); return 1; }
      resolve("file:///Documentos/pagina.html","notas.txt",u,sizeof u);               if(strcmp(u,"file:///Documentos/notas.txt")) { puts(u); puts("FALLO resolve 7"); return 1; }
      resolve("http://example.test","x",u,sizeof u);                                   if(strcmp(u,"http://example.test/x")) { puts(u); puts("FALLO resolve 8"); return 1; }
      puts("RESOLVE OK"); (void)b; }
    { Win *b=&wins[win_top()]; click(165,226); }              /* primer enlace de la lista (ruta absoluta) */
    gui_frame(); gui_frame(); gui_frame(); gui_frame(); gui_frame(); shot("05d_http_enlace");
    { Win *b=&wins[win_top()]; click(b->x+200,b->y+TB+18); typestr("https://segura.test\\"); }
    shot("05e_https_error");
    { Win *b=&wins[win_top()]; click(b->x+200,b->y+TB+18); typestr("fallo.test\\"); gui_frame(); gui_frame(); gui_frame(); gui_frame(); }
    shot("05f_http_fallo");
    closeall();
    dock(0); click(220,648); shot("07_acerca");
    closeall();
    dock(7); { Win *st=&wins[win_top()]; click(st->x+1+20+44+14,st->y+TB+54); click(st->x+1+20+112,st->y+TB+114+30); } shot("08_ajustes_cambio");
    closeall();
    /* buscador */
    dock(1);
    { Win *b=&wins[win_top()]; click(b->x+200,b->y+TB+18); typestr("bienvenida\\"); } shot("09_buscar");
    { Win *b=&wins[win_top()]; click(b->x+200,b->y+TB+18); typestr("vortex\\"); } shot("09b_buscar_vortex");
    { Win *b=&wins[win_top()]; if(strcmp(b->url,"http://10.0.2.2:8080/search?q=vortex")){ puts(b->url); puts("FALLO buscador web url"); return 1; } }
    { Win *b=&wins[win_top()]; click(b->x+200,b->y+TB+18); typestr("https://es.wikipedia.org/wiki/Kernel\\"); if(strncmp(b->url,"http://10.0.2.2:8080/go?u=https%3A%2F%2Fes.wikipedia.org",55)){ puts(b->url); puts("FALLO https por proxy"); return 1; } }
    { FsNode *cf=fs_resolve(fs_root(),"/Sistema/vortex.cfg"); fs_write(cf,"idioma=es\n",10); }                 /* sin buscador web: busqueda local */
    { Win *b=&wins[win_top()]; click(b->x+200,b->y+TB+18); typestr("vortex\\"); if(!strstr(b->url,"vortex://buscar?q=vortex")){ puts(b->url); puts("FALLO buscador local"); return 1; } }
    { Win *b=&wins[win_top()]; click(b->x+200,b->y+TB+18); typestr("zzzzqq nada\\"); } shot("09c_buscar_vacio");
    closeall();
    /* notas */
    dock(4); typestr("Hola desde Notas\nSegunda linea escrita con el teclado de vortex."); { Win *n=&wins[win_top()]; click(n->x+1+86+20,n->y+TB+16); } shot("10_notas");
    { FsNode *f=fs_resolve(fs_root(),"/Documentos/nota.txt"); if(!f||f->size<20){ puts("FALLO guardar nota"); return 1; } }
    closeall();
    /* calculadora */
    dock(5); typestr("12*34="); shot("11_calculadora");
    { Win *c=&wins[win_top()]; if(c->ccur!=408*CSC){ puts("FALLO calc 12*34"); return 1; } typestr("C7/0="); if(!c->cerr){ puts("FALLO calc /0"); return 1; } typestr("C100-1="); if(c->ccur!=99*CSC){ puts("FALLO calc 100-1"); return 1; } }
    closeall();
    /* snake */
    dock(6); gui_key('d'); for(int i=0;i<40;i++){ T+=3; gui_frame(); } shot("12_snake");
    { Win *s2=&wins[win_top()]; if(!s2->sstart){ puts("FALLO snake"); return 1; } }
    for(int i=0;i<400;i++){ T+=3; gui_frame(); } shot("12b_snake_fin");
    closeall();
    /* Imágenes KNGFS malformadas se rechazan ANTES de tocar el árbol. */
    {
      uint32_t gen0=fs_gen();
      FsNode *doc0=fs_resolve(fs_root(),"/Documentos");
      const unsigned char trunc[]={
        1,1,'x',1,0,0,0,'v',2,1,'q',0 /* falta fin del directorio raiz */
      };
      const unsigned char traversal[]={1,2,'.','/',0,0,0,0,0};
      const unsigned char huge[]={1,1,'x',0xff,0xff,0xff,0x7f,0};
      const unsigned char valid[]={1,1,'z',1,0,0,0,'p',0};
      const unsigned char badlink[]={3,1,'q',0xff,0xff,0};
      const unsigned char trailing[]={0,0};
      unsigned char deep[3*70+71];int k=0;
      for(int i=0;i<70;i++){deep[k++]=2;deep[k++]=1;deep[k++]='d';}
      for(int i=0;i<71;i++)deep[k++]=0;
      if(fs_overlay(trunc,sizeof trunc)==0||fs_overlay(traversal,sizeof traversal)==0||
         fs_overlay(huge,sizeof huge)==0||fs_overlay(badlink,sizeof badlink)==0||
         fs_overlay(trailing,sizeof trailing)==0||fs_overlay(deep,sizeof deep)==0||
         fs_deserialize(trunc,sizeof trunc)==0){
         puts("FALLO KNGFS validador de imagenes");return 1;
      }
      if(fs_gen()!=gen0||fs_resolve(fs_root(),"/x")||
         fs_resolve(fs_root(),"/z")||fs_resolve(fs_root(),"/q")||
         fs_resolve(fs_root(),"/Documentos")!=doc0){
         puts("FALLO KNGFS datos modificados por imagen invalida");return 1;
      }
      if(fs_overlay(valid,sizeof valid)!=0||!fs_resolve(fs_root(),"/z")){
         puts("FALLO KNGFS imagen valida");return 1;
      }
      fs_remove(fs_resolve(fs_root(),"/z"));
      puts("VORTEXFS SERIALIZACION: TRUNCADO, LONGITUD, TRAVERSAL, PROFUNDIDAD OK");
    }
    /* Un fichero adoptado no debe truncarse si se modifica al comienzo. */
    {
      static char original[4096];
      for(int i=0;i<4096;i++)original[i]=(char)('A'+i%26);
      FsNode *f=fs_create(fs_root(),"archivo-cow","",0);
      if(!f){puts("FALLO crear archivo copy-on-write");return 1;}
      fs_adopt(f,original,4096);
      if(fs_write_at(f,1,"Z",1)||f->size!=4096||f->ro||
         f->cap<4097||f->data[0]!='A'||f->data[1]!='Z'||
         f->data[2000]!=original[2000]||
         f->data[4095]!=original[4095]||f->data[4096]){
          puts("FALLO copy-on-write truncado o fuera de rango");return 1;
      }
      if(original[1]=='Z'){puts("FALLO modifico buffer de solo lectura");return 1;}
      fs_remove(f);
      puts("VORTEXFS COPY-ON-WRITE COMPLETO OK");
    }
    /* La copia de carpetas debe ser todo o nada ante OOM. */
    {
      FsNode *r=fs_root(),*orig=fs_mkdir(r,"origen-oom");
      if(!orig||!fs_create(orig,"primero.txt","uno",3)||
         !fs_create(orig,"segundo.txt","dos",3)){
         puts("FALLO crear estructura de prueba");return 1;
      }
      for(int allowance=0;allowance<5;allowance++){
          host_alloc_budget=allowance;
          FsNode *copy=fs_copy(orig,r,"destino-oom");
          host_alloc_budget=-1;
          if(copy||fs_find(r,"destino-oom")){
              puts("FALLO copia de carpeta incompleta por falta de memoria");return 1;
          }
      }
      host_alloc_budget=1;
      FsNode *missing=fs_create(r,"archivo-oom","datos",5);
      host_alloc_budget=-1;
      if(missing||fs_find(r,"archivo-oom")){
          puts("FALLO fs_create tras fallo de memoria");return 1;
      }
      FsNode *complete=fs_copy(orig,r,"destino-oom");
      if(!complete||!fs_find(complete,"primero.txt")||!fs_find(complete,"segundo.txt")){
          puts("FALLO copia completa sin OOM");return 1;
      }
      fs_remove(complete);fs_remove(orig);
      puts("VORTEXFS COPIA ATOMICA CON FALLOS DE ASIGNACION OK");
    }
    /* disco y sistema de archivos: formatear, guardar, copiar, mover, renombrar, recargar */
    dock(3); typestr("disco\\disco formatear si\\cd /Documentos\\write a.txt hola disco\\cp a.txt b.txt\\mv b.txt /Escritorio\\ren a.txt z.txt\\mkdir sub\\cp /Sistema sub\\df\\"); shot("17_disco_terminal");
    T+=1000; vortexfs_tick();
    if(!fs_resolve(fs_root(),"/Escritorio/b.txt")||!fs_resolve(fs_root(),"/Documentos/z.txt")||fs_resolve(fs_root(),"/Documentos/a.txt")||!fs_resolve(fs_root(),"/Documentos/sub/Sistema/version.txt")){ puts("FALLO cp/mv/ren"); return 1; }
    closeall();
    { FsNode *z=fs_resolve(fs_root(),"/Documentos/z.txt"); fs_write(z,"CAMBIADO",8); fs_remove(fs_resolve(fs_root(),"/Escritorio/b.txt")); T+=1000; vortexfs_tick(); }
    { fs_write(fs_resolve(fs_root(),"/Documentos/z.txt"),"basura",6); fs_create(fs_root(),"temporal.txt","x",1); }       /* cambios sin guardar: se pierden al "reiniciar" */
    /* Emula un reinicio: la unidad estaba montada y mount() devuelve 0 sin recargar.
     * En el host fake disk persiste; al reiniciar real esta bandera comienza a cero. */
    fs_init(); /* un nuevo arranque recrea primero el filesystem base en RAM */
    mounted=0;
    if(vortexfs_mount()){ puts("FALLO montar"); return 1; }
    { FsNode *z=fs_resolve(fs_root(),"/Documentos/z.txt"); if(!z||z->size!=8||memcmp(z->data,"CAMBIADO",8)||fs_resolve(fs_root(),"/temporal.txt")||fs_resolve(fs_root(),"/Escritorio/b.txt")){ puts("FALLO persistencia"); return 1; } puts("DISCO OK"); }
    /* VortexFS doble snapshot: provocar corte entre datos y cabecera.
     * Debe conservar la version previa, nunca exponer la escritura parcial. */
    {
      FsNode *docs=fs_resolve(fs_root(),"/Documentos");
      if(!docs){puts("FALLO snapshot docs");return 1;}
      FsNode *n=fs_create(docs,"recuperacion.txt","PRIMERO",7);
      if(!n||vortexfs_save()){puts("FALLO guardar checkpoint");return 1;}
      int stable_slot=saved_slot;
      if(!slot_span()||stable_slot<0){puts("FALLO activar doble slot");return 1;}
      if(fs_write(n,"SEGUNDO",7)){puts("FALLO escribir segundo");return 1;}
      fake_fail_after=1; /* escribir los datos, cortar antes de cabecera */
      int failed=vortexfs_save();
      fake_fail_after=-1;
      if(!failed||saved_slot!=stable_slot){puts("FALLO corte de energia simulado");return 1;}
      fs_init();mounted=0;saved_slot=-1;saved_seq=0;last_size=0;
      if(vortexfs_mount()){puts("FALLO recuperacion tras corte");return 1;}
      n=fs_resolve(fs_root(),"/Documentos/recuperacion.txt");
      if(!n||n->size!=7||memcmp(n->data,"PRIMERO",7)){puts("FALLO recuperacion snapshot intacto");return 1;}
      if(fs_write(n,"TERCERO",7)||vortexfs_save()){puts("FALLO siguiente checkpoint");return 1;}
      int new_slot=saved_slot;
      /* Danar la copia mas reciente y verificar fallback al snapshot anterior. */
      fake[(size_t)slot_lba(new_slot)*512+8]^=0xFF;
      fs_init();mounted=0;saved_slot=-1;saved_seq=0;last_size=0;
      if(vortexfs_mount()){puts("FALLO fallback de copia corrupta");return 1;}
      n=fs_resolve(fs_root(),"/Documentos/recuperacion.txt");
      if(!n||n->size!=7||memcmp(n->data,"PRIMERO",7)){puts("FALLO rollback snapshot secundario");return 1;}
      puts("VORTEXFS COPIAS DOBLES: CORTE, CORRUPCION Y RECUPERACION OK");
    }
    dock(8); shot("18_monitor"); closeall();
    /* escritorio modular: arrastrar icono y widget, abrir con doble clic */
    { click(dobj[4].x+42,dobj[4].y+80);
      if(nord<1||wins[win_top()].type!=A_FILES){puts("FALLO acceso rapido Archivos");return 1;}
      closeall();
      click(dobj[4].x+165,dobj[4].y+80);
      if(nord<1||wins[win_top()].type!=A_MEDIA){puts("FALLO acceso rapido Galeria");return 1;}
      closeall();
      click(dobj[5].x+62,dobj[5].y+62);
      if(nord<1||wins[win_top()].type!=A_MONITOR){puts("FALLO tarjeta Monitor");return 1;}
      closeall();
      puts("VORTEX PANEL ACCESOS RAPIDOS INTERACTIVO OK");
    }
    { int ox=dobj[0].x+dobj[0].w/2, oy=dobj[0].y+25; gui_mouse_abs(ox,oy,0); gui_frame(); gui_mouse_abs(ox,oy,1); gui_frame(); gui_mouse_abs(ox+300,oy+200,1); gui_frame(); gui_mouse_abs(ox+300,oy+200,0); gui_frame(); T+=100;
      if(dobj[0].x<200){ puts("FALLO arrastrar icono"); return 1; }
      dclick(ox+300,oy+200); if(nord<1||wins[win_top()].type!=A_BROWSER){ puts("FALLO abrir icono"); return 1; } }
    shot("14_escritorio_modular");
    { Win *b=&wins[win_top()]; int tx=b->x+300, ty=b->y+TB/2; T+=100; click(tx,ty); T+=3; click(tx,ty); if(!b->maxed||b->w!=W){ puts("FALLO maximizar"); return 1; } shot("15_maximizada");
      T+=100; ty=b->y+TB/2; click(tx,ty); T+=3; click(tx,ty); if(b->maxed){ puts("FALLO restaurar"); return 1; }
      T+=100; gui_mouse_abs(b->x+300,b->y+10,0); gui_frame(); gui_mouse_abs(b->x+300,b->y+10,1); gui_frame(); gui_mouse_abs(2,300,1); gui_frame(); gui_mouse_abs(2,300,0); gui_frame();
      if(!b->maxed||b->w!=W/2||b->x!=0){ puts("FALLO ajustar izquierda"); return 1; } shot("16_ajustada"); }
    closeall();
    /* teclado: distribuciones, tildes en terminal/notas/navegador, persistencia y selector en Ajustes */
    { dock(3); typestr("teclado latam\\");
      if(kbd_layout()!=KBD_LATAM){ puts("FALLO teclado latam"); return 1; }
      { FsNode *cf=fs_resolve(fs_root(),"/Sistema/vortex.cfg"); if(!cf||!strstr(cf->data,"teclado=latam")||!strstr(cf->data,"idioma=")){ puts("FALLO cfg teclado"); return 1; } }
      #define SC(b) gui_key(kbd_feed(b))
      SC(0x2A); SC(0x08); SC(0xA8); SC(0xAA);                 /* Shift+7 en Latam = '/' */
      SC(0x27); SC(0xA7);                                     /* n con tilde */
      SC(0x1A); SC(0x9A); SC(0x12); SC(0x92);                 /* tilde muerta + e = e con tilde */
      { Win *t=&wins[win_top()]; unsigned char *g=(unsigned char*)t->grid[t->tr];
        if(t->inlen!=5||memcmp(t->in,"/\xC3\xB1\xC3\xA9",5)||g[t->tc-1]!=0xE9||g[t->tc-2]!=0xF1){ puts("FALLO tildes terminal"); return 1; }
        gui_key('\b'); if(t->inlen!=3){ puts("FALLO borrar tilde"); return 1; } }
      shot("19_terminal_teclado");
      closeall();
      dock(4); SC(0x27); SC(0xA7); SC(0x1E); SC(0x9E); { Win *n=&wins[win_top()]; if(n->nlen!=3||memcmp(n->note,"\xC3\xB1""a",3)){ puts("FALLO nota ñ"); return 1; } gui_key('\b'); gui_key('\b'); if(n->nlen!=0){ puts("FALLO borrar ñ"); return 1; } } closeall();
      kbd_set_layout(KBD_US);
      dock(7); { Win *st=&wins[win_top()]; click(st->x+1+170+60,st->y+TB+50+322+13); if(kbd_layout()!=KBD_LATAM){ puts("FALLO boton teclado"); return 1; } } shot("20_ajustes_teclado"); closeall();
      kbd_set_layout(KBD_US); { char kv[16]; cfg_get("teclado",kv,sizeof kv); kbd_set_layout(layout_from_name(kv)); if(kbd_layout()!=KBD_LATAM){ puts("FALLO leer cfg"); return 1; } }
      gui_panic("Falta de pagina","eip",0x1234abcdULL,0x2,0xdeadbeefULL,1); shot("21_panic"); gui_force_redraw(); puts("TECLADO GUI OK"); }
    /* Espacios de trabajo: cada pantalla conserva sus ventanas y estado. */
    {
      closeall();workspace_set(0);
      Win *a=open_app(A_NOTES);
      if(!a||win_top()!=(int)(a-wins)){puts("FALLO espacio inicial");return 1;}
      click(275,14);
      if(active_workspace!=1||win_top()!=-1||!a->used){
          puts("FALLO espacio 2 aislado");return 1;}
      Win *b=open_app(A_FILES);
      if(!b||b->workspace!=1||win_top()!=(int)(b-wins)){
          puts("FALLO abrir archivo espacio 2");return 1;}
      shot("37_gnome_workspace_dos");
      gui_key(KBD_WORKSPACE_PREV);
      if(active_workspace!=0||win_top()!=(int)(a-wins)||!b->used){
          puts("FALLO recuperar espacio 1");return 1;}
      gui_key(KBD_WORKSPACE_NEXT);
      if(active_workspace!=1||win_top()!=(int)(b-wins)){
          puts("FALLO recuperar espacio 2");return 1;}
      closeall();workspace_set(0);closeall();
      puts("VORTEX GNOME WORKSPACES INDEPENDIENTES OK");
    }
    /* Wheel real: bajo cursor, sin cambiar foco, con limites y sentidos. */
    {
      closeall();workspace_set(0);
      Win *a=open_app(A_FILES),*b=open_app(A_BROWSER);
      if(!a||!b){puts("FALLO ventanas prueba rueda");return 1;}
      FsNode *folder=fs_mkdir(fs_root(),"rueda-test");
      if(!folder){puts("FALLO directorio rueda");return 1;}
      for(int i=0;i<42;i++){char n[24]="fila-";cat_int(n,24,i);
          if(!fs_create(folder,n,"x",1)){puts("FALLO archivos rueda");return 1;}}
      a->dir=folder;a->scroll=0;
      gui_mouse_abs(a->x+25,a->y+a->h/2,0);
      gui_mouse_wheel(-2);
      if(a->scroll!=6||b->bscroll!=0){puts("FALLO scroll archivos bajo cursor");return 1;}
      gui_mouse_wheel(1);
      if(a->scroll!=3){puts("FALLO scroll inverso");return 1;}
      gui_mouse_wheel(8);
      if(a->scroll!=0){puts("FALLO scroll limite superior");return 1;}
      gui_mouse_wheel(-8);
      if(a->scroll<=0||a->scroll>42){puts("FALLO scroll limite inferior");return 1;}
      shot("38_rueda_en_gestor_archivos");
      gui_mouse_abs(b->x+b->w-20,b->y+TB+80,0);
      b->page.content_h=2600;b->bscroll=0;
      gui_mouse_wheel(-1);
      if(b->bscroll!=56){puts("FALLO scroll navegador");return 1;}
      gui_mouse_wheel(1);
      if(b->bscroll!=0){puts("FALLO scroll arriba navegador");return 1;}
      closeall();fs_remove(folder);
      puts("VORTEX INTELLIMOUSE SCROLL VENTANA BAJO CURSOR OK");
    }
    /* La rueda en Notas permite leer el comienzo sin perder lo escrito,
     * y en el visor de imagenes hace zoom reversible con limites. */
    {
      closeall();workspace_set(0);
      Win *n=open_app(A_NOTES);
      if(!n){puts("FALLO abrir Notas con rueda");return 1;}
      n->nlen=0;
      for(int i=0;i<43;i++){
          char line[32]="Linea ";cat_int(line,sizeof line,i);cat(line,sizeof line," texto de prueba\n");
          int len=k_strlen(line);
          if(n->nlen+len>=NOTEMAX){puts("FALLO capacidad nota");return 1;}
          memcpy(n->note+n->nlen,line,(size_t)len);n->nlen+=len;
      }
      n->note[n->nlen]=0;dwin(n);
      gui_mouse_abs(n->x+50,n->y+TB+130,0);
      gui_mouse_wheel(3);
      if(n->scroll!=6){puts("FALLO scroll arriba Notas");return 1;}
      shot("43_rueda_notas");
      gui_mouse_wheel(-4);
      if(n->scroll!=0){puts("FALLO scroll abajo Notas");return 1;}
      closeall();
      puts("VORTEX NOTAS: RUEDA BIDIRECCIONAL OK");
      FsNode *imgs=fs_resolve(fs_root(),"/Imagenes");
      int pnglen=(int)(vortex_demo_png_end-vortex_demo_png_start);
      FsNode *im=imgs?fs_create(imgs,"RuedaVista.png",
                    (const char*)vortex_demo_png_start,pnglen):0;
      if(!im){puts("FALLO imagen demo para zoom");return 1;}
      open_file(im);
      int vi=win_top();
      if(vi<0||wins[vi].type!=A_VIEWER||!wins[vi].picture.pixels){
          puts("FALLO abrir PNG para zoom");return 1;
      }
      Win *v=&wins[vi];
      gui_mouse_abs(v->x+v->w/2,v->y+TB+v->h/2,0);
      gui_mouse_wheel(2);
      if(v->image_zoom!=130){puts("FALLO zoom rueda arriba");return 1;}
      shot("44_zoom_galeria");
      gui_mouse_wheel(-2);
      if(v->image_zoom!=100){puts("FALLO zoom rueda abajo");return 1;}
      gui_mouse_wheel(-8);gui_mouse_wheel(-8);
      if(v->image_zoom!=25){puts("FALLO limite minimo zoom");return 1;}
      gui_mouse_wheel(8);gui_mouse_wheel(8);gui_mouse_wheel(8);
      if(v->image_zoom>400||v->image_zoom<100){
          puts("FALLO limites de zoom");return 1;
      }
      closeall();fs_remove(im);
      puts("VORTEX VISOR PNG: ZOOM REVERSIBLE 25%-400% OK");
    }
    /* Enciclopedia original sin internet: lanzador, indexado, artículos y
     * rueda para desplazarse entre resultados. */
    {
      closeall();workspace_set(0);
      dock(0);typestr("enciclopedia");gui_key('\n');gui_frame();
      int top=win_top();
      if(top<0||wins[top].type!=A_ENCARTA){
          puts("FALLO abrir enciclopedia desde Actividades");return 1;
      }
      Win *e=&wins[top];
      if(ENC_ARTICLES_N<25||enc_count(e)!=ENC_ARTICLES_N){
          puts("FALLO catalogo educativo");return 1;
      }
      shot("39_vortex_saber_biblioteca");
      click(e->x+1+260,e->y+TB+146+19);
      if(e->enc_topic<0||strcmp(ENC_ARTICLES[e->enc_topic].title,"Universo")){
          puts("FALLO abrir articulo educativo");return 1;
      }
      shot("40_vortex_saber_articulo");
      click(e->x+1+238+35,e->y+TB+94+11+13);
      if(e->enc_topic!=-1){puts("FALLO volver al indice");return 1;}
      typestr("inteligencia artificial");
      if(enc_count(e)!=1){puts("FALLO busqueda a texto completo");return 1;}
      gui_key('\n');
      if(e->enc_topic<0||strcmp(ENC_ARTICLES[e->enc_topic].title,"Inteligencia artificial")){
          puts("FALLO enter abre articulo filtrado");return 1;
      }
      gui_key(27);gui_key(27); /* volver y limpiar búsqueda */
      click(e->x+1+54,e->y+TB+137+3*40+14);
      if(e->enc_category!=3||enc_count(e)<4){
          puts("FALLO categoria geografia");return 1;
      }
      /* Geografia cabe completa; para verificar el desplazamiento usamos
       * Todos, que contiene más resultados que filas visibles. */
      click(e->x+1+54,e->y+TB+137+14);
      if(e->enc_category!=0||enc_count(e)!=ENC_ARTICLES_N){
          puts("FALLO regresar a todos los temas");return 1;
      }
      int oldscroll=e->enc_scroll;
      gui_mouse_abs(e->x+1+300,e->y+TB+240,0);
      gui_mouse_wheel(-1);
      if(e->enc_scroll<=oldscroll){
          fprintf(stderr,"RUEDA DEBUG: scroll=%d old=%d topic=%d count=%d active=%d ownspace=%d mouse=(%d,%d) window=(%d,%d,%d,%d) appmenu=%d top=%d nord=%d\n",
                  e->enc_scroll,oldscroll,e->enc_topic,enc_count(e),active_workspace,e->workspace,
                  mx,my,e->x,e->y,e->w,e->h,appmenu_open,win_top(),nord);
          puts("FALLO rueda en enciclopedia");return 1;
      }
      shot("41_vortex_saber_geografia");
      click(e->x+1+54,e->y+TB+94+342+16);
      if(!e->enc_quiz||e->enc_qindex!=0||e->enc_qscore!=0){
          puts("FALLO cuestionario inicial");return 1;
      }
      gui_key('1'); /* primera respuesta correcta: El Sol */
      if(e->enc_qanswer!=0||e->enc_qscore!=1){
          puts("FALLO respuesta cuestionario");return 1;
      }
      gui_key('\n');
      if(e->enc_qindex!=1||e->enc_qanswer!=-1){
          puts("FALLO siguiente pregunta");return 1;
      }
      gui_key('2'); /* ADN */
      if(e->enc_qanswer!=1||e->enc_qscore!=2){
          puts("FALLO puntuacion de quiz");return 1;
      }
      shot("42_vortex_saber_cuestionario");
      gui_key(27);
      if(e->enc_quiz){puts("FALLO salir del cuestionario");return 1;}
      closeall();
      puts("VORTEX SABER: INDICE, ARTICULOS, BUSQUEDA, CATEGORIAS Y SCROLL OK");
    }
    /* Ubuntu/GNOME: dock vertical, Actividades y vista de aplicaciones. */
    { int bx,by,bw,bh;dock_geom(&bx,&by,&bw,&bh);
      if(bx!=DASH_LEFT||bw!=DASH_WIDTH||bh!=NDOCK*DASH_STEP+20||
         by<BARH||by+bh>H){puts("FALLO dock vertical");return 1;}
      closeall();click(53,14);
      if(!appmenu_open){puts("FALLO abrir Actividades desde barra GNOME");return 1;}
      shot("35_actividades_gnome_ubuntu");
      click(53,14);
      if(appmenu_open){puts("FALLO cerrar Actividades");return 1;}
      if(dobj[0].x<=bx+bw){puts("FALLO iconos tapados por dock");return 1;}
      shot("36_escritorio_ubuntu_gnome");
      puts("VORTEX GNOME ACTIVIDADES Y UBUNTU DOCK OK");
    }
    /* La barra conserva el nombre del SO. El primer boton del dock
     * debe abrir Aplicaciones y ofrecer acceso real a LibreOffice. */
    closeall();
    dock(0);
    if(!appmenu_open){puts("FALLO menu Aplicaciones del dock");return 1;}
    shot("22_menu_aplicaciones");
    { int x,y,w,h;launcher_geom(&x,&y,&w,&h);
      appmenu_cat=MC_ALL;
      int target=-1;
      for(int i=0;i<APPMENU_N;i++)
        if(!strcmp(APPMENU[i].label,"LibreOffice")){target=i;break;}
      if(target<0){puts("FALLO LibreOffice ausente del menu");return 1;}
      int cw=(w-177)/4,rh=(h-134)/4;
      int px=x+156+(target%4)*cw+cw/2,py=y+122+(target/4)*rh+rh/2;
      click(px,py);
      if(nord<1||wins[win_top()].type!=A_OFFICE){
          puts("FALLO abrir LibreOffice desde Aplicaciones");return 1;}
      shot("23_centro_libreoffice");
      puts("VORTEX APPS Y LIBREOFFICE GUI OK");
    }
    closeall();
    dock(0); if(!appmenu_open){puts("FALLO launcher cerrado");return 1;}
    { int x,y,w,h;launcher_geom(&x,&y,&w,&h);
      click(x+48,y+76+MC_MULTIMEDIA*42+16);
      if(appmenu_cat!=MC_MULTIMEDIA||!appmenu_open){puts("FALLO categoria multimedia");return 1;}
      shot("24_launcher_categorias");
      int n=appmenu_index(0);
      if(n<0||APPMENU[n].app!=A_MEDIA){puts("FALLO apps multimedia");return 1;}
      click(x+156+(w-177)/8,y+122+(h-134)/8);
      if(nord<1||wins[win_top()].type!=A_MEDIA){puts("FALLO lanzar galeria desde categoria");return 1;}
      closeall();
      puts("VORTEX LAUNCHER CATEGORIAS OK");
    }
    /* Prueba de uso real: buscar aplicaciones y pulsar Enter. */
    { uint64_t before=gui_painted_pixels;
      dock(0);
      if(!appmenu_open){puts("FALLO launcher de busqueda");return 1;}
      uint64_t area=gui_painted_pixels-before;
      if(area>=(uint64_t)W*H){
          fprintf(stderr,"FALLO: launcher repinto toda la pantalla (%llu px)\n",
                  (unsigned long long)area);return 1;
      }
      typestr("calc");
      if(appmenu_index(0)<0||APPMENU[appmenu_index(0)].app!=A_CALC){
          puts("FALLO filtro de busqueda de aplicaciones");return 1;
      }
      shot("28_launcher_busqueda");
      gui_key('\n');gui_frame();
      if(nord<1||wins[win_top()].type!=A_CALC){
          puts("FALLO abrir calculadora con Enter");return 1;
      }
      closeall();
      { uint64_t p=gui_painted_pixels;
        for(int i=0;i<64;i++){gui_mouse_abs(400+i,475,0);gui_frame();}
        if(gui_painted_pixels!=p){
            puts("FALLO cursor mueve y repinta widgets innecesarios");return 1;
        }
      }
      fprintf(stderr,"[vortex-perf] launcher %llu pixeles pintados de %d de pantalla\n",
              (unsigned long long)area,W*H);
      puts("VORTEX BUSQUEDA Y REDIBUJADO INCREMENTAL OK");
    }
    click(W-26,14);
    if(!power_menu_open){puts("FALLO menu energia");return 1;}
    shot("25_menu_energia");
    click(W-128,BARH+18+48+22);
    if(!power_sleep){puts("FALLO suspender pantalla");return 1;}
    shot("26_reposo_pantalla");
    click(W/2,H/2);
    if(power_sleep){puts("FALLO reanudar pantalla");return 1;}
    shot("27_reanudado");
    puts("VORTEX ENERGIA Y REPOSO OK");
    /* Tiempo real de composicion al arrastrar una ventana.
     * Solo es una medicion comparativa en el host, NO fps de VMware. */
    closeall();dock(1);
    { Win *v=&wins[win_top()];
      int px=v->x+60,py=v->y+12;
      gui_mouse_abs(px,py,0);gui_frame();gui_mouse_abs(px,py,1);gui_frame();
      struct timespec t0,t1;
      uint64_t before=gui_painted_pixels;
      clock_gettime(CLOCK_MONOTONIC,&t0);
      for(int j=1;j<=24;j++){
          gui_mouse_abs(px+j*3,py+j,1);gui_frame();
      }
      clock_gettime(CLOCK_MONOTONIC,&t1);
      gui_mouse_abs(px+72,py+24,0);gui_frame();
      double ms=(t1.tv_sec-t0.tv_sec)*1000.0+(t1.tv_nsec-t0.tv_nsec)/1000000.0;
      fprintf(stderr,"[vortex-perf] 24 cuadros arrastrando: %.2f ms total, %.2f ms/cuadro, %llu pixeles/cuadro\n",
          ms,ms/24.0,
          (unsigned long long)((gui_painted_pixels-before)/24u));
      if(ms<=0||ms>60000){puts("FALLO banco de arrastre");return 1;}
    }
    closeall();
    /* escritorio lleno */
    dock(1); dock(2); dock(3); dock(4); dock(5); dock(6); shot("13_escritorio_lleno");
    return 0;
}
