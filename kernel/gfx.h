#ifndef GFX_H
#define GFX_H
#include "vortex.h"
extern int gw, gh;                 /* tamano de pantalla */
extern uint32_t *bb;               /* backbuffer gw*gh, formato 0x00RRGGBB */
int  gfx_init(uint32_t *front, int w, int h, int pitch_bytes);   /* 0 ok */
void gfx_set_bpp(int bpp);
int  gfx_resize(uint32_t *front,int w,int h,int pitch_bytes);                                         /* 32 (normal) o 24 */
void gfx_present(void);
void gfx_front_px(int x, int y, uint32_t c);                     /* escribe directo al frontbuffer (cursor) */
void gfx_present_rect(int x, int y, int w, int h);
void clip_base(int x, int y, int w, int h);   /* recorte maximo (rectangulo sucio); clip_set/clip_reset lo respetan */
void clip_set(int x, int y, int w, int h);
void clip_reset(void);
uint32_t mix(uint32_t a, uint32_t b, int t);                      /* t 0..255: 0=a, 255=b */
void px_blend(int x, int y, uint32_t c, int a);
void fill(int x, int y, int w, int h, uint32_t c, int a);
void blit_scaled32(int x,int y,int w,int h,const uint32_t *src,int sw,int sh);
void blit_bgr_scaled(int x,int y,int w,int h,const uint8_t *src,int sw,int sh,int bits,int topdown);
void blit32(int x, int y, const uint32_t *src, int stride_px, int w, int h);   /* copia pixeles 0x00RRGGBB respetando el recorte */
void rrect(int x, int y, int w, int h, int r, uint32_t c, int a);
void circle(int cx, int cy, int r, uint32_t c, int a);
void poly(const int *xy, int n, uint32_t c, int a);              /* relleno par-impar */
void line(int x0, int y0, int x1, int y1, int t, uint32_t c, int a);
void ellipse_ring(int cx, int cy, int rx, int ry, uint32_t c);
int  text(int x, int y, const char *s, uint32_t c);              /* 8x16, UTF-8 -> Latin-1; devuelve x final */
int  text_bold(int x, int y, const char *s, uint32_t c);
int  text_big(int x, int y, const char *s, uint32_t c);          /* 12x24 */
int  text_w(const char *s);
int  text_ui_w(const char *s);                       /* anchura variable (titulos, launcher, dock) */
int  text_ui(int x,int y,const char *s,uint32_t c);  /* DejaVu con avances proporcionales */
int  text_ui_bold(int x,int y,const char *s,uint32_t c);                                      /* ancho en pixeles (8 px por caracter) */
void crown(int x, int y, int w, uint32_t body, uint32_t bar, int a);   /* logo de la corona; alto = w*2/3 */
#endif
