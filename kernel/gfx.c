/* gfx.c — primitivas 2D por software sobre un backbuffer 32 bpp (sin libc, sin coma flotante). */
#include "gfx.h"
#include "font.h"

int gw, gh; uint32_t *bb;
static uint32_t *front; static int fpitch;               /* pitch del frontbuffer en pixeles */
static int fbpp=32, fpitchb;                              /* tarjetas que solo dan 24 bits por pixel (algunos PC reales): se convierte al copiar */
void gfx_set_bpp(int b){ fbpp=b; }
static int cx0, cy0, cx1, cy1;
static int bx0, by0, bx1, by1;                              /* recorte base (rectangulo sucio) */

/* Evitar overflow, pitch insuficiente y asignaciones desmesuradas
 * al aceptar resoluciones que anuncia el firmware o la tarjeta virtual. */
static int gfx_valid_mode(const uint32_t *f,int w,int h,int pitch_bytes){
    if(!f||w<=0||h<=0||w>8192||h>4320||pitch_bytes<=0)return 0;
    if(fbpp!=24&&fbpp!=32)return 0;
    if((size_t)w>(size_t)-1/(size_t)h/4u)return 0;
    if((size_t)pitch_bytes<(size_t)w*(size_t)(fbpp/8))return 0;
    if(fbpp==32&&(pitch_bytes&3))return 0;
    return 1;
}
int gfx_resize(uint32_t *f,int w,int h,int pitch_bytes){
    if(!gfx_valid_mode(f,w,h,pitch_bytes))return -1;
    size_t bytes=(size_t)w*(size_t)h*4u;
    uint32_t *n=kmalloc(bytes);if(!n)return -1;
    /* Un fallo de validacion/asignacion deja intacta la pantalla anterior. */
    if(bb)kfree(bb);
    bb=n;front=f;gw=w;gh=h;fpitch=pitch_bytes/4;fpitchb=pitch_bytes;
    memset(bb,0,bytes);clip_base(0,0,w,h);return 0;
}
int gfx_init(uint32_t *f,int w,int h,int pitch_bytes){
    if(!gfx_valid_mode(f,w,h,pitch_bytes))return -1;
    size_t bytes=(size_t)w*(size_t)h*4u;
    uint32_t *n=kmalloc(bytes);if(!n)return -1;
    bb=n;front=f;gw=w;gh=h;fpitch=pitch_bytes/4;fpitchb=pitch_bytes;
    memset(bb,0,bytes);clip_base(0,0,w,h);return 0;
}
void gfx_present_rect(int x,int y,int w,int h){
    if(!bb||!front||w<=0||h<=0)return;
    /* Sumas en 64 bits para no desbordar con coordenadas corruptas. */
    int64_t rx=(int64_t)x+w,ry=(int64_t)y+h;
    if(rx<=0||ry<=0||x>=gw||y>=gh)return;
    int x0=k_max(0,x),y0=k_max(0,y);
    int x1=rx>=gw?gw:(int)rx,y1=ry>=gh?gh:(int)ry;
    if(x1<=x0||y1<=y0)return;
    x=x0;y=y0;w=x1-x0;h=y1-y0;
    if(fbpp==24){
        for(int j=0;j<h;j++){ uint8_t *d=(uint8_t*)front+(size_t)(y+j)*fpitchb+(size_t)x*3; const uint32_t *s2=bb+(y+j)*gw+x;
            for(int i=0;i<w;i++){ uint32_t c=s2[i]; d[0]=(uint8_t)c; d[1]=(uint8_t)(c>>8); d[2]=(uint8_t)(c>>16); d+=3; } }
        return; }
    for(int j=0;j<h;j++) memcpy(front+(y+j)*fpitch+x, bb+(y+j)*gw+x, (size_t)w*4);
}
void gfx_front_px(int x,int y,uint32_t c){ if(!front||x<0||x>=gw||y<0||y>=gh) return;
    if(fbpp==24){ uint8_t *d=(uint8_t*)front+(size_t)y*fpitchb+(size_t)x*3; d[0]=(uint8_t)c; d[1]=(uint8_t)(c>>8); d[2]=(uint8_t)(c>>16); return; }
    front[y*fpitch+x]=c; }
void gfx_present(void){ gfx_present_rect(0,0,gw,gh); }
void clip_base(int x,int y,int w,int h){ bx0=k_max(x,0); by0=k_max(y,0); bx1=k_min(x+w,gw); by1=k_min(y+h,gh); clip_reset(); }
void clip_set(int x,int y,int w,int h){ cx0=k_max(k_max(x,0),bx0); cy0=k_max(k_max(y,0),by0); cx1=k_min(k_min(x+w,gw),bx1); cy1=k_min(k_min(y+h,gh),by1); }
void clip_reset(void){ cx0=bx0; cy0=by0; cx1=bx1; cy1=by1; }

uint32_t mix(uint32_t a,uint32_t b,int t){
    if(t<=0) return a;
    if(t>=255) return b;
    t+=t>>7;
    uint32_t rb=(((a&0xFF00FF)*(256-t)+(b&0xFF00FF)*t)>>8)&0xFF00FF;
    uint32_t g =(((a&0x00FF00)*(256-t)+(b&0x00FF00)*t)>>8)&0x00FF00;
    return rb|g;
}
void px_blend(int x,int y,uint32_t c,int a){
    if(x<cx0||x>=cx1||y<cy0||y>=cy1||a<=0) return;
    uint32_t *p=&bb[y*gw+x]; *p = a>=255 ? c : mix(*p,c,a);
}
void blit32(int x,int y,const uint32_t *src,int stride,int w,int h){
    int sx=0, sy=0;
    if(x<cx0){ sx=cx0-x; } if(y<cy0){ sy=cy0-y; }
    int x1=k_min(x+w,cx1), y1=k_min(y+h,cy1); int dx=k_max(x,cx0), dy=k_max(y,cy0);
    if(x1<=dx||y1<=dy) return;
    for(int j=dy;j<y1;j++) memcpy(&bb[j*gw+dx],&src[(sy+(j-dy))*stride+sx],(size_t)(x1-dx)*4);
}
/* Blit escalado por nearest neighbour con clip directo. Menos llamadas y
 * conversiones de pixeles que dibujar cada cuadro via px_blend(). */
void blit_scaled32(int x,int y,int w,int h,const uint32_t *src,int sw,int sh){
    if(!src||w<=0||h<=0||sw<=0||sh<=0)return;
    int x0=k_max(x,cx0),x1=k_min(x+w,cx1);
    int y0=k_max(y,cy0),y1=k_min(y+h,cy1);
    if(x1<=x0||y1<=y0)return;
    for(int yy=y0;yy<y1;yy++){
        int sy=((yy-y)*sh)/h;if(sy>=sh)sy=sh-1;
        const uint32_t *r=src+(size_t)sy*sw;
        uint32_t *d=bb+(size_t)yy*gw;
        for(int xx=x0;xx<x1;xx++){
            int sx=((xx-x)*sw)/w;if(sx>=sw)sx=sw-1;
            d[xx]=r[sx];
        }
    }
}
void blit_bgr_scaled(int x,int y,int w,int h,const uint8_t *src,int sw,int sh,int bits,int topdown){
    if(!src||w<=0||h<=0||sw<=0||sh<=0||(bits!=24&&bits!=32))return;
    int x0=k_max(x,cx0),x1=k_min(x+w,cx1);
    int y0=k_max(y,cy0),y1=k_min(y+h,cy1);
    if(x1<=x0||y1<=y0)return;
    unsigned bytepp=(unsigned)bits/8u;
    size_t stride=((size_t)sw*bytepp+3u)&~3u;
    for(int yy=y0;yy<y1;yy++){
        int sy=((yy-y)*sh)/h;if(sy>=sh)sy=sh-1;
        if(!topdown)sy=sh-1-sy;
        const uint8_t *r=src+(size_t)sy*stride;
        uint32_t *d=bb+(size_t)yy*gw;
        for(int xx=x0;xx<x1;xx++){
            int sx=((xx-x)*sw)/w;if(sx>=sw)sx=sw-1;
            const uint8_t *p=r+(size_t)sx*bytepp;
            d[xx]=((uint32_t)p[2]<<16)|((uint32_t)p[1]<<8)|p[0];
        }
    }
}
void fill(int x,int y,int w,int h,uint32_t c,int a){
    int x1=k_min(x+w,cx1), y1=k_min(y+h,cy1); x=k_max(x,cx0); y=k_max(y,cy0);
    if(a<=0) return;
    for(int j=y;j<y1;j++){ uint32_t *p=&bb[j*gw];
        if(a>=255) for(int i=x;i<x1;i++) p[i]=c; else for(int i=x;i<x1;i++) p[i]=mix(p[i],c,a); }
}
/* cobertura 0..16 (4x4 submuestras) del pixel (i,j) respecto a un circulo de radio r con centro en (cx4,cy4)/4 */
static int cov(int i,int j,int cx4,int cy4,int r){
    int r4=r*4, r2=r4*r4;
    int dx=i*4+2-cx4, dy=j*4+2-cy4, d2=dx*dx+dy*dy;
    int in=(r4-3)>0?(r4-3)*(r4-3):0, out=(r4+3)*(r4+3);       /* el pixel mide ~2.8 submuestras de diagonal */
    if(d2<=in) return 16;
    if(d2>=out) return 0;
    int k=0;
    for(int sy=0;sy<4;sy++) for(int sx=0;sx<4;sx++){
        int ex=i*4+sx-cx4, ey=j*4+sy-cy4;
        if((2*ex+1)*(2*ex+1)+(2*ey+1)*(2*ey+1)<=4*r2) k++;
    }
    return k;
}
void rrect(int x,int y,int w,int h,int r,uint32_t c,int a){
    if(r*2>w) r=w/2;
    if(r*2>h) r=h/2;
    if(r<=0){ fill(x,y,w,h,c,a); return; }
    fill(x+r,y,w-2*r,h,c,a); fill(x,y+r,r,h-2*r,c,a); fill(x+w-r,y+r,r,h-2*r,c,a);
    for(int j=0;j<r;j++) for(int i=0;i<r;i++){
        int k=cov(i,j,r*4,r*4,r); if(!k) continue; int al=a*k/16;
        px_blend(x+i,y+j,c,al); px_blend(x+w-1-i,y+j,c,al);
        px_blend(x+i,y+h-1-j,c,al); px_blend(x+w-1-i,y+h-1-j,c,al);
    }
}
void circle(int cx,int cy,int r,uint32_t c,int a){
    for(int j=-r;j<=r;j++) for(int i=-r;i<=r;i++){
        int k=cov(i+r,j+r,r*4+2,r*4+2,r); /* centro en (r+0.5) */
        if(k) px_blend(cx+i,cy+j,c,a*k/16);
    }
}
void poly(const int *xy,int n,uint32_t c,int a){
    int ymin=xy[1], ymax=xy[1];
    for(int i=1;i<n;i++){ ymin=k_min(ymin,xy[2*i+1]); ymax=k_max(ymax,xy[2*i+1]); }
    for(int y=ymin;y<=ymax;y++){
        int xs[32], m=0;
        for(int i=0;i<n;i++){
            int x0=xy[2*i],y0=xy[2*i+1],x1=xy[2*((i+1)%n)],y1=xy[2*((i+1)%n)+1];
            if(y0==y1) continue;
            if(y0>y1){ int t=x0; x0=x1; x1=t; t=y0; y0=y1; y1=t; }
            if(y<y0||y>=y1) continue;
            if(m<32) xs[m++]=x0+(x1-x0)*(y-y0)/(y1-y0);
        }
        for(int i=1;i<m;i++){ int v=xs[i],j=i-1; while(j>=0&&xs[j]>v){ xs[j+1]=xs[j]; j--; } xs[j+1]=v; }
        for(int i=0;i+1<m;i+=2) fill(xs[i],y,xs[i+1]-xs[i],1,c,a);
    }
}
void line(int x0,int y0,int x1,int y1,int t,uint32_t c,int a){
    int dx=x1>x0?x1-x0:x0-x1, sx=x0<x1?1:-1, dy=-(y1>y0?y1-y0:y0-y1), sy=y0<y1?1:-1, e=dx+dy;
    for(;;){
        fill(x0-t/2,y0-t/2,t,t,c,a);
        if(x0==x1&&y0==y1) break;
        int e2=2*e;
        if(e2>=dy){ e+=dy; x0+=sx; }
        if(e2<=dx){ e+=dx; y0+=sy; }
    }
}
static const short COS16[16]={1000,924,707,383,0,-383,-707,-924,-1000,-924,-707,-383,0,383,707,924};
static const short SIN16[16]={0,383,707,924,1000,924,707,383,0,-383,-707,-924,-1000,-924,-707,-383};
void ellipse_ring(int cx,int cy,int rx,int ry,uint32_t c){
    for(int i=0;i<16;i++){ int j=(i+1)&15;
        line(cx+rx*COS16[i]/1000,cy+ry*SIN16[i]/1000,cx+rx*COS16[j]/1000,cy+ry*SIN16[j]/1000,2,c,255); }
}

/* ---- texto ---- */
static int next_char(const char **p){
    unsigned char c=(unsigned char)**p; (*p)++;
    if(c<0x80) return c;
    if((c==0xC2||c==0xC3) && **p){ int d=(unsigned char)**p; (*p)++; return ((c&3)<<6)|(d&0x3F); }
    while(((unsigned char)**p&0xC0)==0x80) (*p)++;            /* otro UTF-8: se salta */
    return '?';
}
int text_w(const char *s){ int n=0; while(*s){ next_char(&s); n++; } return n*8; }
static inline int gam(int a){ return a+(((255-a)*a)>>9); }
static void glyph(int x,int y,int ch,uint32_t c){
    if(ch<32||ch>255) ch='?';
    if(x+8<=cx0||x>=cx1||y+16<=cy0||y>=cy1) return;
    const unsigned char *g=font_s[ch-32];
    for(int j=0;j<16;j++){ int yy=y+j; if(yy<cy0||yy>=cy1) continue;
        uint32_t *p=bb+yy*gw; const unsigned char *r=g+j*8;
        for(int i=0;i<8;i++){ int a=r[i]; int xx=x+i; if(!a||xx<cx0||xx>=cx1) continue; a=gam(a); p[xx]= a>=250 ? c : mix(p[xx],c,a); } }
}
int text(int x,int y,const char *s,uint32_t c){ while(*s){ int ch=next_char(&s); glyph(x,y,ch,c); x+=8; } return x; }
int text_bold(int x,int y,const char *s,uint32_t c){ text(x+1,y,s,c); return text(x,y,s,c); }
/* Los antiguos widgets se dibujaban en monospace 8px fijo. Para la interfaz
 * principal usamos la misma fuente antialias con espaciado proporcional:
 * un 'i' no ocupa lo mismo que una 'W'. La consola conserva 8x16 fijo. */
static unsigned char ui_left[224],ui_step[224];
static int ui_font_ready;
static void ui_font_init(void){
    if(ui_font_ready)return;
    for(int ch=32;ch<=255;ch++){
        const unsigned char *g=font_s[ch-32];
        int left=8,right=-1;
        for(int x=0;x<8;x++){
            int ink=0;
            for(int y=0;y<16;y++)if(g[y*8+x]>=18){ink=1;break;}
            if(ink){if(x<left)left=x;if(x>right)right=x;}
        }
        if(right<0){ui_left[ch-32]=0;ui_step[ch-32]=4;}
        else{
            ui_left[ch-32]=(unsigned char)left;
            ui_step[ch-32]=(unsigned char)k_min(9,right-left+2);
        }
    }
    ui_font_ready=1;
}
int text_ui_w(const char *s){
    ui_font_init();
    int w=0;
    while(*s){
        int ch=next_char(&s);
        if(ch<32||ch>255)ch='?';
        w+=ui_step[ch-32];
    }
    return w;
}
int text_ui(int x,int y,const char *s,uint32_t c){
    ui_font_init();
    while(*s){
        int ch=next_char(&s);
        if(ch<32||ch>255)ch='?';
        glyph(x-ui_left[ch-32],y,ch,c);
        x+=ui_step[ch-32];
    }
    return x;
}
int text_ui_bold(int x,int y,const char *s,uint32_t c){
    text_ui(x+1,y,s,c);
    return text_ui(x,y,s,c);
}
int text_big(int x,int y,const char *s,uint32_t c){
    while(*s){ int ch=next_char(&s); if(ch<32||ch>255) ch='?';
        const unsigned char *g=font_b[ch-32];
        for(int j=0;j<24;j++){ int yy=y+j; if(yy<cy0||yy>=cy1) continue;
            for(int i=0;i<12;i++){ int a=g[j*12+i]; if(a) px_blend(x+i,yy,c,gam(a)); } }
        x+=12; }
    return x;
}
/* ---- logo: corona de tres puntas con bolitas y base (cuadricula de diseno 180x120) ---- */
void crown(int x,int y,int w,uint32_t body,uint32_t bar,int a){
    static const int P[]={10,100, 20,26, 55,66, 90,14, 125,66, 160,26, 170,100};
    int q[14]; for(int i=0;i<7;i++){ q[2*i]=x+P[2*i]*w/180; q[2*i+1]=y+P[2*i+1]*w/180; }
    poly(q,7,body,a);
    circle(x+20*w/180,y+20*w/180,k_max(2,8*w/180),body,a);
    circle(x+90*w/180,y+9*w/180,k_max(2,9*w/180),body,a);
    circle(x+160*w/180,y+20*w/180,k_max(2,8*w/180),body,a);
    rrect(x+10*w/180,y+104*w/180,160*w/180,k_max(3,14*w/180),k_max(2,7*w/180),bar,a);
}
