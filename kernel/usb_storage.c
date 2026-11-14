/* Vortex OS USB Mass Storage nativo: UHCI (PCI), BOT/SCSI, FAT32 lectura.
 * No se usa BIOS ni Linux. Soporta raiz USB fisica conectada a controlador UHCI.
 * No escribe jamas al USB. Los archivos importados son una instantanea de la unidad.
 */
#include "usb_storage.h"
#include "fs.h"
#include "vortex.h"
#include "fat32_layout.h"
#include "exfat_layout.h"
#ifdef HOSTGUI
static int ready;
int usb_storage_probe(void){return 0;}
int usb_storage_ready(void){return ready;}
const char *usb_storage_message(void){return "No hay USB fisico en el simulador host";}
void usb_storage_poll(void){}
#else
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
static inline void o8(u16 p,u8 v){__asm__ volatile("outb %0,%1"::"a"(v),"Nd"(p));}
static inline void o16(u16 p,u16 v){__asm__ volatile("outw %0,%1"::"a"(v),"Nd"(p));}
static inline void o32(u16 p,u32 v){__asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p));}
static inline u8 i8(u16 p){u8 r;__asm__ volatile("inb %1,%0":"=a"(r):"Nd"(p));return r;}
static inline u16 i16(u16 p){u16 r;__asm__ volatile("inw %1,%0":"=a"(r):"Nd"(p));return r;}
static inline u32 i32(u16 p){u32 r;__asm__ volatile("inl %1,%0":"=a"(r):"Nd"(p));return r;}
static u32 pci_rd(int b,int d,int f,int reg){
    o32(0xCF8,0x80000000u|((u32)b<<16)|((u32)d<<11)|((u32)f<<8)|((u32)reg&0xfcu));
    return i32(0xCFC);
}
static void pci_wr(int b,int d,int f,int reg,u32 val){
    o32(0xCF8,0x80000000u|((u32)b<<16)|((u32)d<<11)|((u32)f<<8)|((u32)reg&0xfcu));
    o32(0xCFC,val);
}
static u16 io,port;static int low,found,ready,ever,probe_once;
static u8 address=1,epin,epout,mpin=64,mpout=64,mpzero=8,tgin,tgout,ifnum;
static char info[120]="Sin memoria USB FAT32 reconocida";
typedef struct {volatile u32 link,sts,token,buf,software[4];} TD;
typedef struct {volatile u32 head,element,soft[2];} QH;
static volatile u32 frame[1024] __attribute__((aligned(4096)));
static TD tds[70] __attribute__((aligned(16)));
static QH qh __attribute__((aligned(16)));
static u8 setup[8] __attribute__((aligned(16)));
static u8 payload[4096] __attribute__((aligned(16)));
static u32 scsi_tag=0x10736u,block_count,part,first_data,fat_sector,root_cluster;
static u32 fat_size,reserved,sectors_per_cluster,nfats,last_fat_lba=0xffffffffu;
static u32 volume_end,cluster_count;
static int exfat_mode;
static u8 fatcache[512];
static int imported,depth_count;
static u32 rd32(const u8 *x){return (u32)x[0]|((u32)x[1]<<8)|(u32)x[2]<<16|(u32)x[3]<<24;}
static u16 rd16(const u8 *x){return (u16)x[0]|(u16)x[1]<<8;}
static void le32(u8 *x,u32 v){x[0]=(u8)v;x[1]=(u8)(v>>8);x[2]=(u8)(v>>16);x[3]=(u8)(v>>24);}
static void waitms(int ms){u32 st=kticks();while((int)(kticks()-st)<ms/10+1){}}
static void td_set(int i,int pid,u8 dev,int ep,int toggle,u32 len,const u8 *buf){
    TD *t=&tds[i];
    t->link=1;t->sts=(1u<<23)|(3u<<27)|(low?(1u<<26):0u);
    t->token=(u32)pid|((u32)dev<<8)|((u32)ep<<15)|((u32)toggle<<19)|
             ((len?len-1u:0x7ffu)<<21);
    t->buf=(u32)(uintptr_t)buf;
}
static int run(int n){
    if(n<1||n>70)return -1;
    for(int i=0;i<n-1;i++)tds[i].link=(u32)(uintptr_t)&tds[i+1];
    tds[n-1].link=1;
    __asm__ volatile("":::"memory");
    qh.element=(u32)(uintptr_t)&tds[0];
    u32 start=kticks();
    for(int spin=0;spin<100000000;spin++){
        if(!(tds[n-1].sts&(1u<<23)))break;
        if((int)(kticks()-start)>150){qh.element=1;return -2;}
        int fail=0;for(int i=0;i<n;i++)if(tds[i].sts&0x007e0000u)fail=1;
        if(fail){qh.element=1;return -3;}
    }
    int ok=!(tds[n-1].sts&(1u<<23));
    for(int i=0;i<n;i++)if(tds[i].sts&0x007e0000u)ok=0;
    qh.element=1;return ok?0:-4;
}
static int ctrl(u8 dev,u8 type,u8 req,u16 value,u16 index,u8 *data,int n,int in){
    if(n<0||n>256)return -1;
    setup[0]=type;setup[1]=req;setup[2]=(u8)value;setup[3]=(u8)(value>>8);
    setup[4]=(u8)index;setup[5]=(u8)(index>>8);setup[6]=(u8)n;setup[7]=(u8)(n>>8);
    td_set(0,0x2d,dev,0,0,8,setup);
    int k=1,toggle=1;
    for(int off=0;off<n;){
        int c=n-off;if(c>mpzero)c=mpzero;
        td_set(k++,in?0x69:0xe1,dev,0,toggle,c,data+off);
        toggle^=1;off+=c;
    }
    td_set(k,in?0xe1:0x69,dev,0,1,0,setup);
    return run(k+1);
}
static int bulk(int input,u8 ep,u8 *data,int n,u8 maxp,u8 *toggle){
    if(n<0||n>4096||maxp==0)return -1;
    int k=0;
    for(int off=0;off<n;){
        int c=n-off;if(c>maxp)c=maxp;
        td_set(k++,input?0x69:0xe1,address,ep,*toggle,c,data+off);
        *toggle^=1;off+=c;
        if(k>=70)return -1;
    }
    return run(k);
}
static int bot(const u8 *cdb,int cdblen,u8 *data,int length){
    if(length<0||length>4096)return -1;
    u8 cbw[31];memset(cbw,0,sizeof cbw);memcpy(cbw,"USBC",4);
    u32 tag=++scsi_tag;le32(cbw+4,tag);le32(cbw+8,(u32)length);
    cbw[12]=length?0x80:0;cbw[14]=(u8)cdblen;memcpy(cbw+15,cdb,(size_t)cdblen);
    if(bulk(0,epout,cbw,31,mpout,&tgout))return -1;
    if(length&&bulk(1,epin,data,length,mpin,&tgin))return -2;
    u8 csw[16];memset(csw,0,sizeof csw);
    if(bulk(1,epin,csw,13,mpin,&tgin))return -3;
    if(rd32(csw)!=0x53425355u||rd32(csw+4)!=tag||csw[12]||rd32(csw+8))return -4;
    return 0;
}
static void bot_recover(void){
    /* BOT Reset de clase, CLEAR_FEATURE(HALT) y restablecer DATA0 */
    (void)ctrl(address,0x21,0xff,0,ifnum,0,0,0);
    (void)ctrl(address,0x02,1,0,(u16)(epin|0x80),0,0,0);
    (void)ctrl(address,0x02,1,0,epout,0,0,0);
    tgin=tgout=0;
    waitms(15);
}
static int readsector(u32 lba,u8 *out){
    if(lba>=block_count)return -1;
    u8 cdb[10]={0x28,0,0,0,0,0,0,0,1,0};
    cdb[2]=(u8)(lba>>24);cdb[3]=(u8)(lba>>16);cdb[4]=(u8)(lba>>8);cdb[5]=(u8)lba;
    u8 tmp[512];
    for(int attempt=0;attempt<3;attempt++){
        int r=bot(cdb,10,tmp,512);
        if(!r){memcpy(out,tmp,512);return 0;}
        kprintf("[vortex-usb] reintento READ10 lba=%u error=%d intento=%d\n",lba,r,attempt+1);
        bot_recover();
    }
    return -1;
}
static int controller(void){
    if(io)return 0;
    for(int b=0;b<256;b++)for(int d=0;d<32;d++){
        if((pci_rd(b,d,0,0)&0xffffu)==0xffffu)continue;
        int nf=(pci_rd(b,d,0,12)&0x00800000u)?8:1;
        for(int f=0;f<nf;f++){
            if((pci_rd(b,d,f,0)&0xffffu)==0xffffu)continue;
            if((pci_rd(b,d,f,8)>>8)!=0x0c0300u)continue;
            u32 bar=pci_rd(b,d,f,0x20);
            if(!(bar&1u))continue;
            io=(u16)(bar&~3u);
            if(!io)continue;
            pci_wr(b,d,f,4,pci_rd(b,d,f,4)|5u);
            return 0;
        }
    }
    return -1;
}
static int init(void){
    if(controller())return -1;
    o16(io+0,2);waitms(20);o16(io+0,0);
    o16(io+4,0);o16(io+2,0x3f);o16(io+6,0);
    qh.head=1;qh.element=1;
    for(int i=0;i<1024;i++)frame[i]=((u32)(uintptr_t)&qh)|2u;
    o32(io+8,(u32)(uintptr_t)frame);
    o8(io+0x0c,0x40);
    o16(io+0,0x00c1); /* RUN/STOP, Configure Flag, Max Packet */
    waitms(40);return 0;
}
static int portreset(void){
    for(int p=0;p<2;p++){
        u16 a=(u16)(io+0x10+p*2);
        if(!(i16(a)&1))continue;
        o16(a,0x0200);waitms(60);
        o16(a,0);waitms(20);
        o16(a,0x0006);waitms(15);
        u16 s=i16(a);
        if(!(s&4))continue;
        low=!!(s&0x0100);port=a;return 0;
    }
    return -1;
}
static int enumerate(void){
    u8 desc[256];memset(desc,0,sizeof desc);
    mpzero=8;address=1;
    if(ctrl(0,0x80,6,0x0100,0,desc,8,1))return -1;
    if(desc[0]<8||desc[1]!=1)return -2;
    mpzero=desc[7];if(mpzero!=8&&mpzero!=16&&mpzero!=32&&mpzero!=64)return -3;
    if(ctrl(0,0,5,address,0,0,0,0))return -4;
    waitms(20);
    if(ctrl(address,0x80,6,0x0200,0,desc,9,1))return -5;
    if(desc[1]!=2)return -6;
    int n=rd16(desc+2);if(n<18||n>256)return -7;
    if(ctrl(address,0x80,6,0x0200,0,desc,n,1))return -8;
    int conf=desc[5],classok=0;epin=epout=0;
    for(int p=0;p+2<=n;){
        int len=desc[p],type=desc[p+1];if(len<2||p+len>n)break;
        if(type==4&&len>=9){
            classok=desc[p+5]==8&&desc[p+6]==6&&desc[p+7]==0x50;
            if(classok)ifnum=desc[p+2];
        }
        if(type==5&&len>=7&&classok&&(desc[p+3]&3)==2){
            int m=rd16(desc+p+4);if(m<=0||m>64){p+=len;continue;}
            if(desc[p+2]&0x80){epin=desc[p+2]&15;mpin=(u8)m;}
            else{epout=desc[p+2]&15;mpout=(u8)m;}
        }
        p+=len;
    }
    if(!epin||!epout)return -9;
    if(ctrl(address,0,9,(u16)conf,0,0,0,0))return -10;
    tgin=tgout=0;
    u8 cd[10]={0x25};u8 cap[8];
    if(bot(cd,10,cap,8))return -11;
    block_count=((u32)cap[0]<<24)|((u32)cap[1]<<16)|((u32)cap[2]<<8)|cap[3];
    u32 last=block_count;
    u32 bs=((u32)cap[4]<<24)|((u32)cap[5]<<16)|((u32)cap[6]<<8)|cap[7];
    if(bs!=512||last==0xffffffffu||last<127)return -12;
    block_count=last+1u;
    return 0;
}
static int fatmount(void){
    u8 s[512];if(readsector(0,s))return -1;
    part=0;exfat_mode=0;u32 part_sectors=block_count;
    if(s[510]==0x55&&s[511]==0xAA){
        for(int i=0;i<4;i++){const u8 *e=s+446+i*16;
            if(e[4]==0x0b||e[4]==0x0c||e[4]==0x07){
                u32 p=rd32(e+8),cnt=rd32(e+12);
                if(p&&p<block_count&&cnt&&cnt<=block_count-p){
                    part=p;part_sectors=cnt;break;
                }
            }
        }
        if(part&&readsector(part,s))return -2;
    }
    VortexExfatLayout ex;
    if(!vortex_exfat_layout(s,block_count,part,part_sectors,&ex)){
        exfat_mode=1;sectors_per_cluster=ex.sectors_per_cluster;
        root_cluster=ex.root_cluster;volume_end=ex.volume_end;cluster_count=ex.cluster_count;
        fat_sector=ex.fat_lba;fat_size=ex.fat_sectors;first_data=ex.first_data;
        last_fat_lba=0xffffffffu;
        kprintf("[vortex-usb] exFAT part=%u root=%u spc=%u fat=%u data=%u end=%u\n",
            part,root_cluster,sectors_per_cluster,fat_sector,first_data,volume_end);
        return 0;
    }
    VortexFat32Layout v;
    if(vortex_fat32_layout(s,block_count,part,part_sectors,&v))return -3;
    sectors_per_cluster=v.sectors_per_cluster;
    reserved=rd16(s+14);nfats=s[16];fat_size=v.fat_sectors;
    root_cluster=v.root_cluster;volume_end=v.volume_end;cluster_count=v.cluster_count;
    fat_sector=v.fat_lba;first_data=v.first_data;
    last_fat_lba=0xffffffffu;
    kprintf("[vortex-usb] BPB part=%u root=%u spc=%u fat=%u first=%u end=%u\n",
            part,root_cluster,sectors_per_cluster,fat_sector,first_data,volume_end);
    return 0;
}
static u32 fatnext(u32 cluster){
    if(cluster<2||(u64)cluster>=(u64)cluster_count+2u||cluster/128u>=fat_size)
        return 0x0fffffffu;
    u32 lba=fat_sector+cluster/128u;
    if(lba>=volume_end)return 0x0fffffffu;
    if(lba!=last_fat_lba){if(readsector(lba,fatcache))return 0x0fffffffu;last_fat_lba=lba;}
    u32 value=rd32(fatcache+(cluster%128u)*4u);
    return exfat_mode?value:(value&0x0fffffffu);
}
static int cluster_lba(u32 cl,u32 *out){
    if(cl<2||(u64)cl>=(u64)cluster_count+2u)return -1;
    u64 lba=(u64)first_data+(u64)(cl-2u)*sectors_per_cluster;
    if(lba+sectors_per_cluster>volume_end)return -1;
    *out=(u32)lba;return 0;
}
static int goodname(const char *s){
    if(!s[0]||s[0]=='.'||k_strlen(s)>=FS_NAME)return 0;
    for(;*s;s++)if(*s=='/'||*s=='\\'||(unsigned char)*s<32)return 0;
    return 1;
}
static void shortname(const u8 *e,char *name){
    int i=0;for(int k=0;k<8&&e[k]!=' ';k++)name[i++]=(char)e[k];
    if(e[8]!=' '){name[i++]='.';for(int k=8;k<11&&e[k]!=' ';k++)name[i++]=(char)e[k];}
    name[i]=0;
}
static void longpart(const u8 *e,char *dst){
    int order=e[0]&31;if(order<1||order>6)return;
    static const int offsets[13]={1,3,5,7,9,14,16,18,20,22,24,28,30};
    int base=(order-1)*13;
    for(int k=0;k<13&&base+k<FS_NAME-1;k++){
        int cp=rd16(e+offsets[k]);if(cp==0||cp==0xffff){dst[base+k]=0;break;}
        dst[base+k]=cp>=32&&cp<127?(char)cp:'_';
    }
}
static int readfile(u32 cl,u32 len,FsNode *dir,const char *name){
    if(!len||len>4u*1024u*1024u||imported>=100)return -1;
    u8 *buf=kmalloc((size_t)len);if(!buf)return -1;
    u32 got=0,guard=0;
    for(u32 c=cl;got<len&&c>=2&&c<0x0ffffff8u&&guard++<2048;c=fatnext(c)){
        u32 base;if(cluster_lba(c,&base))break;
        for(u32 j=0;j<sectors_per_cluster&&got<len;j++){
            u8 sec[512];if(readsector(base+j,sec)){got=0;break;}
            u32 take=len-got;if(take>512)take=512;
            memcpy(buf+got,sec,take);got+=take;
        }
    }
    if(got!=len)kprintf("[vortex-usb] lectura incompleta %s: %u/%u bytes\n",name,got,len);
    if(got==len){FsNode *f=fs_create(dir,name,(char*)buf,(int)len);
        if(f){f->dev=1;imported++;kprintf("[vortex-usb] archivo %s (%u bytes) importado\n",name,len);}
        else kprintf("[vortex-usb] sin memoria para el archivo %s\n",name);}
    kfree(buf);
    return got==len?0:-1;
}
static void readdir_usb(u32 first,FsNode *dst,int depth){
    if(depth>4||depth_count++>120)return;
    char longname[FS_NAME];memset(longname,0,sizeof longname);
    u32 cnt=0;
    for(u32 c=first;c>=2&&c<0x0ffffff8u&&cnt++<256;c=fatnext(c)){
        u32 base;if(cluster_lba(c,&base))break;
        for(u32 s=0;s<sectors_per_cluster;s++){
            u8 sec[512];if(readsector(base+s,sec))return;
            if(depth==0&&cnt==1&&s==0)kprintf("[vortex-usb] raiz lba=%u e0=%x attr=%x e1=%x\n",base+s,(u32)sec[0],(u32)sec[11],(u32)sec[32]);
            for(int j=0;j<16;j++){
                const u8 *e=sec+j*32;
                if(e[0]==0)return;
                if(e[0]==0xe5){longname[0]=0;continue;}
                if(e[11]==0x0f){if(e[0]&0x40)memset(longname,0,sizeof longname);
                    longpart(e,longname);continue;}
                if(e[11]&8){longname[0]=0;continue;}
                char nm[FS_NAME];if(longname[0])k_strcpy(nm,longname,FS_NAME);
                else shortname(e,nm);
                longname[0]=0;
                if(!goodname(nm))continue;
                u32 start=((u32)rd16(e+20)<<16)|rd16(e+26),len=rd32(e+28);
                if((e[11]&16)&&depth<4){
                    FsNode *child=fs_mkdir(dst,nm);if(child){child->dev=1;readdir_usb(start,child,depth+1);}
                }else if(!(e[11]&16)&&len<=4u*1024u*1024u){
                    (void)readfile(start,len,dst,nm);
                }
            }
        }
    }
}
/* UTF-16LE from exFAT filename entries to UTF-8, without truncating
 * multi-byte characters. Unsupported surrogate pairs are skipped. */
static void exfat_name_piece(const u8 *e,char *name,int *used,int *chars,int maxchars){
    for(int j=0;j<15&&*chars<maxchars;j++){
        u32 cp=rd16(e+2+j*2);(*chars)++;
        if(!cp||cp==0xffffu||cp>=0xd800u&&cp<=0xdfffu)continue;
        int bytes=cp<0x80?1:cp<0x800?2:3;
        if(*used+bytes>=FS_NAME){*used=FS_NAME;return;}
        if(bytes==1)name[(*used)++]=(char)cp;
        else if(bytes==2){name[(*used)++]=(char)(0xc0|(cp>>6));name[(*used)++]=(char)(0x80|(cp&63));}
        else{name[(*used)++]=(char)(0xe0|(cp>>12));name[(*used)++]=(char)(0x80|((cp>>6)&63));name[(*used)++]=(char)(0x80|(cp&63));}
        name[*used]=0;
    }
}
static int readfile_exfat(u32 first,u32 len,int contiguous,FsNode *dir,const char *name){
    if(!len||len>4u*1024u*1024u||imported>=100||first<2)return -1;
    u8 *buf=kmalloc(len);if(!buf)return -1;
    u32 got=0,visited=0;
    for(u32 c=first;got<len&&visited++<8192;){
        u32 base;if(cluster_lba(c,&base))break;
        for(u32 j=0;j<sectors_per_cluster&&got<len;j++){
            u8 sec[512];if(readsector(base+j,sec)){got=0;break;}
            u32 take=len-got;if(take>512)take=512;
            memcpy(buf+got,sec,take);got+=take;
        }
        if(contiguous){if(c>=cluster_count+1u)break;c++;}
        else {u32 next=fatnext(c);if(next<2||next>=0xfffffff8u)break;c=next;}
    }
    if(got==len){FsNode *f=fs_create(dir,name,(const char*)buf,(int)len);
        if(f){f->dev=1;imported++;kprintf("[vortex-usb] archivo %s (%u bytes) importado exFAT\n",name,len);}}
    kfree(buf);return got==len?0:-1;
}
static void readdir_exfat(u32 first,int contiguous,u64 data_length,FsNode *dir,int depth){
    if(depth>4||depth_count++>120||first<2)return;
    u32 visited=0,seen=0;
    int rem=0,have_stream=0,nchars=0,name_used=0,expected=0;
    u32 attr=0,file_cluster=0;u64 file_length=0;
    int nofat=0;
    char name[FS_NAME];name[0]=0;
    for(u32 c=first;visited++<256;){
        u32 base;if(cluster_lba(c,&base))return;
        for(u32 j=0;j<sectors_per_cluster;j++){
            if(data_length&&((u64)seen*512u)>=data_length)return;
            u8 sec[512];if(readsector(base+j,sec))return;
            seen++;
            for(int k=0;k<16;k++){
                const u8 *e=sec+k*32;
                if(!e[0])return; /* fin del directorio */
                if(e[0]==0x85){
                    rem=e[1];attr=rd16(e+4);have_stream=0;
                    file_cluster=0;file_length=0;nofat=0;
                    nchars=name_used=expected=0;name[0]=0;
                    if(rem>18)rem=0;
                    continue;
                }
                if(!rem)continue;
                if(e[0]==0xc0&&!have_stream){
                    have_stream=1;nofat=!!(e[1]&2);
                    expected=e[3];file_cluster=rd32(e+20);
                    file_length=ex_rd64(e+24);
                }else if(e[0]==0xc1&&have_stream&&name_used<FS_NAME){
                    exfat_name_piece(e,name,&name_used,&nchars,expected);
                }
                rem--;
                if(rem==0){
                    if(!have_stream||nchars!=expected||!goodname(name)||
                       file_cluster<2||file_cluster>=cluster_count+2u)continue;
                    if(attr&0x10){
                        if(depth<4&&file_length<=8u*1024u*1024u){
                            FsNode *sub=fs_mkdir(dir,name);
                            if(sub){sub->dev=1;readdir_exfat(file_cluster,nofat,file_length,sub,depth+1);}
                        }
                    }else if(file_length<=4u*1024u*1024u&&file_length>0){
                        (void)readfile_exfat(file_cluster,(u32)file_length,nofat,dir,name);
                    }
                }
            }
        }
        if(contiguous){if(c>=cluster_count+1u)return;c++;}
        else {u32 next=fatnext(c);if(next<2||next>=0xfffffff8u)return;c=next;}
    }
}
static void status(const char *msg){k_strcpy(info,msg,sizeof info);kprintf("[vortex-usb] %s\n",info);}
int usb_storage_probe(void){
    if(ready)return 1;
    probe_once=1;
    if(init()){status("USB: no hay controlador UHCI disponible");return 0;}
    if(portreset()){status("UHCI detectado, sin dispositivo conectado");return 0;}
    if(enumerate()){status("USB conectado, pero no es almacenamiento BOT/SCSI compatible");return -1;}
    if(fatmount()){status("USB detectado: se requiere FAT32 o exFAT en UHCI");return -2;}
    FsNode *root=fs_root();
    FsNode *mount=fs_find(root,"Dispositivos");
    if(!mount)mount=fs_mkdir(root,"Dispositivos");
    if(!mount){status("Sin memoria para montar USB");return -3;}
    mount->dev=1;
    FsNode *dir=fs_find(mount,"Unidad-USB");
    if(!dir)dir=fs_mkdir(mount,"Unidad-USB");
    if(!dir){status("Sin memoria para mostrar USB");return -4;}
    dir->dev=1;imported=depth_count=0;
    if(exfat_mode)readdir_exfat(root_cluster,0,0,dir,0);
    else readdir_usb(root_cluster,dir,0);
    ready=1;found=1;ever=1;
    kprintf("[vortex-usb] importados=%d entradas, raiz=%u\n",imported,root_cluster);
    status(exfat_mode?"USB exFAT real leido en /Dispositivos/Unidad-USB (solo lectura)":
                      "USB FAT32 real leido en /Dispositivos/Unidad-USB (solo lectura)");
    return 1;
}
int usb_storage_ready(void){return ready;}
const char *usb_storage_message(void){return info;}
void usb_storage_poll(void){
    static u32 next_probe;
    u32 now=kticks();
    if(!ready){
        /* Evitar congelar la GUI: sin dispositivo en los puertos UHCI no
         * reiniciar la controladora ni recorrer FAT32 cada cinco segundos. */
        if(!io&&probe_once)return; /* no aparece PCI UHCI por conectar pendrive */
        if(io&&!((i16((u16)(io+0x10))|i16((u16)(io+0x12)))&1u))return;
        if((int)(now-next_probe)>=0){
            next_probe=now+500;
            (void)usb_storage_probe();
        }
        return;
    }
    if(!io||!port)return;
    if(!(i16(port)&1)){
        ready=0;
        next_probe=now+200;
        status("USB desconectado; archivos mostrados son una instantanea");
    }
}
#endif
