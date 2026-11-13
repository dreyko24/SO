/* Visor de imagenes nativo (BMP, PNG, PPM) para los dos kernels propios. */
#include "image.h"
#include "zip.h"
#define VIMAGE_MAX_DIM 2048
#define VIMAGE_MAX_IDAT (12u*1024u*1024u)
static uint32_t rdle(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint32_t rdbe(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|(uint32_t)p[3];}
static int dims(uint32_t w,uint32_t h){return w>0&&h>0&&w<=VIMAGE_MAX_DIM&&h<=VIMAGE_MAX_DIM&&w*h<=4194304u;}
static uint32_t color(uint32_t rgb,int alpha){
    uint32_t bg=0x121526;
    if(alpha>=255)return rgb;
    if(alpha<=0)return bg;
    int r=(((rgb>>16)&255)*alpha+((bg>>16)&255)*(255-alpha))/255;
    int g=(((rgb>>8)&255)*alpha+((bg>>8)&255)*(255-alpha))/255;
    int b=((rgb&255)*alpha+(bg&255)*(255-alpha))/255;
    return (uint32_t)r<<16|(uint32_t)g<<8|(uint32_t)b;
}
void vimage_release(VImage *im){if(im&&im->pixels)kfree(im->pixels);if(im){im->pixels=0;im->w=0;im->h=0;}}
static int png_decode(const uint8_t *d,int n,VImage *out){
    static const uint8_t sig[8]={137,80,78,71,13,10,26,10};
    if(n<45)return -1;for(int i=0;i<8;i++)if(d[i]!=sig[i])return -1;
    uint32_t w=0,h=0,ct=0,depth=0,idat=0;int seen=0,interlace=0,haveEnd=0;
    uint8_t palette[768]={0},trans[256];int palN=0;
    for(int i=0;i<256;i++)trans[i]=255;
    for(uint32_t pos=8;pos+12u<=(uint32_t)n;){
        uint32_t len=rdbe(d+pos);pos+=4;uint32_t kind=rdbe(d+pos);pos+=4;
        if(len>(uint32_t)n-pos-4u)return -1;
        const uint8_t *src=d+pos;
        if(kind==0x49484452u){ /* IHDR */
            if(seen||len!=13)return -1;seen=1;w=rdbe(src);h=rdbe(src+4);
            depth=src[8];ct=src[9];interlace=src[12];
            if(!dims(w,h)||src[10]!=0||src[11]!=0||interlace!=0)return -1;
            if(depth!=8 && !((ct==0||ct==3)&&(depth==1||depth==2||depth==4)))return -1;
            if(ct!=0&&ct!=2&&ct!=3&&ct!=4&&ct!=6)return -1;
        }else if(kind==0x504c5445u){
            if(len>768||len%3)return -1;
            memcpy(palette,src,len);palN=(int)len/3;
        }else if(kind==0x74524e53u){
            if(len>256)return -1;memcpy(trans,src,len);
        }else if(kind==0x49444154u){
            if(!seen||len>VIMAGE_MAX_IDAT-idat)return -1;idat+=len;
        }else if(kind==0x49454e44u){haveEnd=1;break;}
        pos+=len+4; /* CRC */
    }
    if(!seen||!haveEnd||!idat||(ct==3&&!palN))return -1;
    int channels=ct==6?4:ct==2?3:ct==4?2:1;
    int filter_bpp=channels*(int)depth/8;if(filter_bpp<1)filter_bpp=1;
    uint32_t stride=(w*(uint32_t)channels*depth+7u)/8u;
    if(stride>=0x7fffffffu||h>(0x7fffffffu/(stride+1u)))return -1;
    uint32_t rawlen=(stride+1u)*h;
    uint8_t *comp=kmalloc(idat),*raw=kmalloc(rawlen);
    if(!comp||!raw){if(comp)kfree(comp);if(raw)kfree(raw);return -1;}
    uint32_t got=0;
    for(uint32_t pos=8;pos+12u<=(uint32_t)n;){
        uint32_t len=rdbe(d+pos),kind=rdbe(d+pos+4);pos+=8;
        if(len>(uint32_t)n-pos-4u)break;
        if(kind==0x49444154u){memcpy(comp+got,d+pos,len);got+=len;}
        if(kind==0x49454e44u)break;
        pos+=len+4;
    }
    int decoded=zip_inflate_zlib(comp,(int)got,raw,(int)rawlen);
    kfree(comp);
    if(decoded!=(int)rawlen){kfree(raw);return -1;}
    uint32_t *px=kmalloc((size_t)w*h*4u);
    if(!px){kfree(raw);return -1;}
    for(uint32_t y=0;y<h;y++){
        uint8_t *r=raw+y*(stride+1u)+1;
        uint8_t *prev=y?raw+(y-1u)*(stride+1u)+1:0;
        int f=r[-1];if(f>4){kfree(raw);kfree(px);return -1;}
        for(uint32_t j=0;j<stride;j++){
            int left=j>=(uint32_t)filter_bpp?r[j-filter_bpp]:0;
            int up=prev?prev[j]:0;
            int ul=prev&&j>=(uint32_t)filter_bpp?prev[j-filter_bpp]:0;
            int add=0;
            if(f==1)add=left;else if(f==2)add=up;else if(f==3)add=(left+up)/2;
            else if(f==4){int p=left+up-ul,pa=p-left,pb=p-up,pc=p-ul;
                if(pa<0)pa=-pa;if(pb<0)pb=-pb;if(pc<0)pc=-pc;
                add=pa<=pb&&pa<=pc?left:pb<=pc?up:ul;}
            r[j]=(uint8_t)(r[j]+add);
        }
        for(uint32_t x=0;x<w;x++){
            uint32_t c=0;int a=255;uint8_t *q=r+x*(uint32_t)channels;
            int packed=0;
            if(depth<8){uint32_t bit=x*depth;packed=(r[bit>>3]>>(8u-depth-(bit&7u)))&((1<<depth)-1);}
            if(ct==6||ct==2){c=((uint32_t)q[0]<<16)|((uint32_t)q[1]<<8)|q[2];if(ct==6)a=q[3];}
            else if(ct==4){c=(uint32_t)q[0]*0x010101u;a=q[1];}
            else if(ct==3){int idx=depth==8?q[0]:packed;if(idx>=palN){kfree(raw);kfree(px);return -1;}
                c=((uint32_t)palette[idx*3]<<16)|((uint32_t)palette[idx*3+1]<<8)|palette[idx*3+2];a=trans[idx];}
            else {int v=depth==8?q[0]:(packed*255/((1<<depth)-1));c=(uint32_t)v*0x010101u;}
            px[y*w+x]=color(c,a);
        }
    }
    kfree(raw);out->w=(int)w;out->h=(int)h;out->pixels=px;return 0;
}
static int bmp_decode(const uint8_t *d,int n,VImage *out){
    if(n<54||d[0]!='B'||d[1]!='M')return -1;
    uint32_t off=rdle(d+10),dib=rdle(d+14);if(dib<40||off>=(uint32_t)n)return -1;
    int32_t w=(int32_t)rdle(d+18),h=(int32_t)rdle(d+22);
    int topdown=h<0;if(topdown)h=-h;
    if(w<=0||h<=0||!dims((uint32_t)w,(uint32_t)h)||d[26]!=1||d[27]!=0||rdle(d+30)!=0)return -1;
    int bpp=(int)d[28]|((int)d[29]<<8);if(bpp!=24&&bpp!=32)return -1;
    uint32_t row=(((uint32_t)w*(uint32_t)(bpp/8))+3u)&~3u;
    if((uint64_t)off+(uint64_t)row*(uint32_t)h>(uint64_t)n)return -1;
    uint32_t *px=kmalloc((size_t)w*h*4u);if(!px)return -1;
    for(int y=0;y<h;y++){
        const uint8_t *src=d+off+(uint32_t)(topdown?y:h-1-y)*row;
        for(int x=0;x<w;x++){const uint8_t *p=src+x*(bpp/8);
            px[y*w+x]=((uint32_t)p[2]<<16)|((uint32_t)p[1]<<8)|p[0];}
    }
    out->w=w;out->h=h;out->pixels=px;return 0;
}
static int ppm_read(const uint8_t *d,int n,int *pos){
    while(*pos<n){uint8_t c=d[*pos];if(c=='#'){while(*pos<n&&d[*pos]!='\n')(*pos)++;continue;}
        if(c==' '||c=='\n'||c=='\r'||c=='\t'){(*pos)++;continue;}break;}
    int v=0,count=0;while(*pos<n&&d[*pos]>='0'&&d[*pos]<='9'&&count<8){v=v*10+d[(*pos)++]-'0';count++;}
    return count?v:-1;
}
static int ppm_decode(const uint8_t *d,int n,VImage *out){
    if(n<12||d[0]!='P'||d[1]!='6')return -1;
    int pos=2,w=ppm_read(d,n,&pos),h=ppm_read(d,n,&pos),max=ppm_read(d,n,&pos);
    if(w<=0||h<=0||!dims((uint32_t)w,(uint32_t)h)||max!=255||pos>=n)return -1;
    if(d[pos]!=' '&&d[pos]!='\n'&&d[pos]!='\r'&&d[pos]!='\t')return -1;
    pos++;if((uint64_t)pos+(uint64_t)w*h*3u>(uint64_t)n)return -1;
    uint32_t *px=kmalloc((size_t)w*h*4u);if(!px)return -1;
    for(int i=0;i<w*h;i++){const uint8_t *p=d+pos+i*3;px[i]=((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];}
    out->w=w;out->h=h;out->pixels=px;return 0;
}
int vimage_decode(const unsigned char *d,int n,VImage *out){
    if(!out)return -1;out->pixels=0;out->w=out->h=0;
    if(!d||n<=0)return -1;
    if(n>=8&&d[0]==137&&d[1]=='P'&&d[2]=='N'&&d[3]=='G')return png_decode(d,n,out);
    if(n>=2&&d[0]=='B'&&d[1]=='M')return bmp_decode(d,n,out);
    if(n>=2&&d[0]=='P'&&d[1]=='6')return ppm_decode(d,n,out);
    return -1;
}
