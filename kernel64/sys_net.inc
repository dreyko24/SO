/* sys_net.inc — sockets, poll/select/epoll, eventfd, timerfd, dispositivos (/dev) y archivos generados (/proc, /sys).
 * Se incluye dentro de proc64.c (comparte sus tipos y funciones estaticas). */

#define PIN   0x001
#define POUT  0x004
#define PERR  0x008
#define PHUP  0x010
#define PNVAL 0x020
enum { EBADMSG_=74, ENOTSOCK=88, EAFNOSUPPORT=97, EINPROGRESS=115, EWOULDBLOCK_=11, EPERM=1, ENOTCONN=107, EOPNOTSUPP=95, ENODEV=19, ESPIPE=29, EACCES=13 };
enum { DEV_NULL=1, DEV_ZERO, DEV_RANDOM, DEV_TTY, DEV_FB, DEV_EVENT, DEV_FULL };

static int fd_events_p(Proc *pr,int fdn);
static int w_fd(Task *t){ return fd_events_p(t->pr,t->wa_fd)&(t->wa_mask|PERR|PHUP|PNVAL); }
static int wait_fd(int fdn,int mask,u32 tmo){ cur->wa_fd=fdn; cur->wa_mask=mask; return block_fn(w_fd,tmo); }

/* ---------- aleatorios ---------- */
static u64 rnd_state=0x9E3779B97F4A7C15ull;
static void rnd_fill(u8 *b,u64 n){ rnd_state^=kticks(); u64 t; __asm__ volatile("rdtsc":"=A"(t)); rnd_state^=t; for(u64 i=0;i<n;i++){ rnd_state^=rnd_state<<13; rnd_state^=rnd_state>>7; rnd_state^=rnd_state<<17; b[i]=(u8)rnd_state; } }

/* ---------- direcciones de socket ---------- */
static int get_sockaddr(u64 va,u64 len,int *fam,u32 *ip,u16 *port,char *up,int *pl){
    u8 b[128]; if(len<2||len>128) return -EINVAL; if(ucopy_from(CP,b,va,len)) return -EFAULT;
    int f=b[0]|(b[1]<<8); *fam=f; *pl=0;
    if(f==2){ if(len<8) return -EINVAL; *port=(u16)((b[2]<<8)|b[3]); *ip=((u32)b[4]<<24)|((u32)b[5]<<16)|((u32)b[6]<<8)|b[7]; return 0; }
    if(f==1){ int l=(int)len-2; if(l>108) l=108; if(l>0&&b[2]!=0){ int k=0; while(k<l&&b[2+k]) k++; l=k; } if(l>0) memcpy(up,b+2,(size_t)l); *pl=l; return 0; }
    return -EAFNOSUPPORT;
}
static int put_sockaddr(u64 va,u64 lenva,int fam,u32 ip,u16 port,const char *up,int pl){
    if(!va||!lenva) return 0;
    u8 b[128]; memset(b,0,sizeof b); int n;
    if(fam==2){ b[0]=2; b[2]=(u8)(port>>8); b[3]=(u8)port; b[4]=(u8)(ip>>24); b[5]=(u8)(ip>>16); b[6]=(u8)(ip>>8); b[7]=(u8)ip; n=16; }
    else { b[0]=1; if(pl>108) pl=108; if(pl) memcpy(b+2,up,(size_t)pl); n=2+pl+((pl&&up[0])?1:0); }
    u32 have=0; if(ucopy_from(CP,&have,lenva,4)) return -EFAULT;
    u32 c=have<(u32)n?have:(u32)n; if(c&&ucopy_to(CP,va,b,c)) return -EFAULT;
    u32 nn=(u32)n; return ucopy_to(CP,lenva,&nn,4)?-EFAULT:0;
}
static int fd_sock(u64 fdn){ if(fdn>=MAXFD||CP->fd[fdn].type!=5) return -1; return CP->fd[fdn].aux; }

static u8 netbuf[16384];
#define MSG_DONTWAIT 0x40
#define MSG_NOSIGNAL 0x4000
#define MSG_PEEK 0x2
#define MSG_CTRUNC 0x8
#define MSG_CMSG_CLOEXEC 0x40000000

static u64 sock_recv_fd(u64 fdn,u64 buf,u64 n,u64 flags,u64 addr,u64 alen){
    FD *f=&CP->fd[fdn]; int s=f->aux; int nb=(f->flags&0x800)||(flags&MSG_DONTWAIT);
    if(n>sizeof netbuf) n=sizeof netbuf;
    for(;;){
        u32 ip=0; u16 port=0; int r=sock_recv(s,netbuf,(int)n,(flags&MSG_PEEK)!=0,&ip,&port,0);
        if(r>=0){
            if(r&&ucopy_to(CP,buf,netbuf,(u64)r)) return RET_ERR(EFAULT);
            if(addr){ char up[4]; int fam=sock_family(s); (void)up; if(fam==2) put_sockaddr(addr,alen,2,ip,port,"",0); else put_sockaddr(addr,alen,1,0,0,"",0); }
            return (u64)r;
        }
        if(r!=-EAGAIN) return RET_ERR(-r);
        if(nb) return RET_ERR(EAGAIN);
        if(INTR()) return RET_ERR(EINTR);
        int b=wait_fd((int)fdn,PIN,f->tmo_r); if(b==1) return RET_ERR(EINTR); if(b==2) return RET_ERR(EAGAIN);
    }
}
static u64 sock_send_fd(u64 fdn,u64 buf,u64 n,u64 flags,int have_dst,u32 ip,u16 port,const char *up,int pl){
    FD *f=&CP->fd[fdn]; int s=f->aux; int nb=(f->flags&0x800)||(flags&MSG_DONTWAIT); u64 done=0;
    if(sock_type(s)==SK_DGRAM){
        if(n>sizeof netbuf) n=sizeof netbuf;
        if(n&&ucopy_from(CP,netbuf,buf,n)) return RET_ERR(EFAULT);
        int r=sock_send(s,netbuf,(int)n,ip,port,up,pl,have_dst,0); return r<0?RET_ERR(-r):(u64)r;
    }
    if(!n) { int r=sock_send(s,netbuf,0,0,0,0,0,0,0); return r<0?RET_ERR(-r):0; }
    while(done<n){
        u64 c=n-done; if(c>sizeof netbuf) c=sizeof netbuf;
        if(ucopy_from(CP,netbuf,buf+done,c)) return done?done:RET_ERR(EFAULT);
        int r=sock_send(s,netbuf,(int)c,0,0,0,0,0,0);
        if(r>0){ done+=(u64)r; if((u64)r<c&&nb) break; continue; }
        if(r==-EAGAIN){
            if(nb) return done?done:RET_ERR(EAGAIN);
            if(INTR()) return done?done:RET_ERR(EINTR);
            int b=wait_fd((int)fdn,POUT,f->tmo_s); if(b==1) return done?done:RET_ERR(EINTR); if(b==2) return done?done:RET_ERR(EAGAIN);
            continue; }
        if(r==-EPIPE){ if(!(flags&MSG_NOSIGNAL)) sig_self(13); return done?done:RET_ERR(EPIPE); }
        return done?done:RET_ERR(-r);
    }
    return done;
}
static u64 sys_socket(u64 dom,u64 type,u64 proto){
    (void)proto;
    int base=(int)(type&0xF); if(dom==10||dom==16||dom==17||dom==38) return RET_ERR(EAFNOSUPPORT);
    if(dom!=1&&dom!=2) return RET_ERR(EAFNOSUPPORT);
    if(base==5&&dom==1) base=1;                                            /* SOCK_SEQPACKET local: se trata como flujo (Chromium lo usa para hablar con sus procesos) */
    if(base!=1&&base!=2) return RET_ERR(base==3?EPERM:22);
    int fd=fd_alloc(CP,0); if(fd<0) return RET_ERR(EMFILE);
    int s=sock_create((int)dom,base); if(s<0) return RET_ERR(-s);
    FD *f=&CP->fd[fd]; memset(f,0,sizeof *f); f->type=5; f->aux=s; f->flags=(type&0x800)?0x800:0; f->cloexec=(type&0x80000)?1:0;
    return (u64)fd;
}
static u64 sys_socketpair(u64 dom,u64 type,u64 proto,u64 sv){
    (void)proto; if(dom!=1) return RET_ERR(EAFNOSUPPORT);
    int base=(int)(type&0xF); if(base==5) base=1; if(base!=1&&base!=2) return RET_ERR(22);
    int f0=fd_alloc(CP,0); if(f0<0) return RET_ERR(EMFILE); CP->fd[f0].type=9; int f1=fd_alloc(CP,0); CP->fd[f0].type=0; if(f1<0) return RET_ERR(EMFILE);
    int a,b; int r=sock_pair(base,&a,&b); if(r<0) return RET_ERR(-r);
    int fl=(type&0x800)?0x800:0, ce=(type&0x80000)?1:0;
    memset(&CP->fd[f0],0,sizeof(FD)); memset(&CP->fd[f1],0,sizeof(FD));
    CP->fd[f0].type=5; CP->fd[f0].aux=a; CP->fd[f0].flags=fl; CP->fd[f0].cloexec=(u8)ce;
    CP->fd[f1].type=5; CP->fd[f1].aux=b; CP->fd[f1].flags=fl; CP->fd[f1].cloexec=(u8)ce;
    int fds[2]={f0,f1}; return ucopy_to(CP,sv,fds,8)?RET_ERR(EFAULT):0;
}
static u64 sys_bind(u64 fdn,u64 addr,u64 len){
    int s=fd_sock(fdn); if(s<0) return RET_ERR(fdn<MAXFD&&CP->fd[fdn].type?ENOTSOCK:EBADF);
    int fam; u32 ip=0; u16 port=0; char up[108]; int pl;
    int r=get_sockaddr(addr,len,&fam,&ip,&port,up,&pl); if(r<0) return RET_ERR(-r);
    if(fam!=sock_family(s)) return RET_ERR(EAFNOSUPPORT);
    r=sock_bind(s,ip,port,up,pl); if(r<0) return RET_ERR(-r);
    if(fam==1&&pl>0&&up[0]){ char path[110]; memcpy(path,up,(size_t)pl); path[pl]=0; char leaf[FS_NAME]; FsNode *d=resolve_parent(path,leaf); if(d&&d->dir&&!fs_find(d,leaf)){ FsNode *n=fs_create(d,leaf,"",0); if(n){ n->sys=1; n->dev=8; } } }
    return 0;
}
static u64 sys_listen(u64 fdn,u64 backlog){ int s=fd_sock(fdn); if(s<0) return RET_ERR(fdn<MAXFD&&CP->fd[fdn].type?ENOTSOCK:EBADF); int r=sock_listen(s,(int)backlog); return r<0?RET_ERR(-r):0; }
static u64 sys_connect(u64 fdn,u64 addr,u64 len){
    int s=fd_sock(fdn); if(s<0) return RET_ERR(fdn<MAXFD&&CP->fd[fdn].type?ENOTSOCK:EBADF);
    int fam; u32 ip=0; u16 port=0; char up[108]; int pl;
    int r=get_sockaddr(addr,len,&fam,&ip,&port,up,&pl); if(r<0) return RET_ERR(-r);
    if(fam!=sock_family(s)) return RET_ERR(EAFNOSUPPORT);
    r=sock_connect(s,ip,port,up,pl);
    if(r!=-EINPROGRESS) return r<0?RET_ERR(-r):0;
    if(CP->fd[fdn].flags&0x800) return RET_ERR(EINPROGRESS);
    while(sock_connecting(s)){
        if(INTR()) return RET_ERR(EINTR);
        int b=wait_fd((int)fdn,POUT,0); if(b==1) return RET_ERR(EINTR);
    }
    int e=sock_take_err(s); return e?RET_ERR(e):0;
}
static u64 sys_accept(u64 fdn,u64 addr,u64 alen,u64 flags){
    int s=fd_sock(fdn); if(s<0) return RET_ERR(fdn<MAXFD&&CP->fd[fdn].type?ENOTSOCK:EBADF);
    int nb=CP->fd[fdn].flags&0x800;
    for(;;){
        u32 ip=0; u16 port=0; int c=sock_accept(s,&ip,&port);
        if(c>=0){
            int nf=fd_alloc(CP,0); if(nf<0){ sock_unref(c); return RET_ERR(EMFILE); }
            FD *f=&CP->fd[nf]; memset(f,0,sizeof *f); f->type=5; f->aux=c; f->flags=(flags&0x800)?0x800:0; f->cloexec=(flags&0x80000)?1:0;
            if(addr){ int fam=sock_family(c); char up[108]; int pl=0; if(fam==1){ u32 i2; u16 p2; sock_names(c,1,&i2,&p2,up,&pl); } put_sockaddr(addr,alen,fam,ip,port,up,pl); }
            return (u64)nf; }
        if(c!=-EAGAIN) return RET_ERR(-c);
        if(nb) return RET_ERR(EAGAIN);
        if(INTR()) return RET_ERR(EINTR);
        int b=wait_fd((int)fdn,PIN,0); if(b==1) return RET_ERR(EINTR);
    }
}
static u64 sys_getsockname(u64 fdn,u64 addr,u64 alen,int peer){
    int s=fd_sock(fdn); if(s<0) return RET_ERR(fdn<MAXFD&&CP->fd[fdn].type?ENOTSOCK:EBADF);
    u32 ip=0; u16 port=0; char up[108]; int pl=0; int fam=sock_names(s,peer,&ip,&port,up,&pl); if(fam<0) return RET_ERR(-fam);
    int r=put_sockaddr(addr,alen,fam,ip,port,up,pl); return r<0?RET_ERR(-r):0;
}
static u64 sys_setsockopt(u64 fdn,u64 level,u64 opt,u64 val,u64 len){
    int s=fd_sock(fdn); if(s<0) return RET_ERR(fdn<MAXFD&&CP->fd[fdn].type?ENOTSOCK:EBADF);
    u32 v=0; if(val&&len>=4) ucopy_from(CP,&v,val,4);
    if(level==1&&opt==2) sock_set_reuse(s,v!=0);
    if(level==1&&(opt==20||opt==21)&&val&&len>=16){ u64 t[2]; if(!ucopy_from(CP,t,val,16)){ u32 tk=(u32)(t[0]*100+t[1]/10000); if(opt==20) CP->fd[fdn].tmo_r=tk; else CP->fd[fdn].tmo_s=tk; } }
    return 0;
}
static u64 sys_getsockopt(u64 fdn,u64 level,u64 opt,u64 val,u64 lenva){
    int s=fd_sock(fdn); if(s<0) return RET_ERR(fdn<MAXFD&&CP->fd[fdn].type?ENOTSOCK:EBADF);
    u32 v=0;
    if(level==1){ if(opt==4) v=(u32)sock_take_err(s); else if(opt==3) v=(u32)sock_type(s); else if(opt==7||opt==8) v=65536; else if(opt==39) v=(u32)sock_family(s);
        else if(opt==17){ u32 uc[3]={(u32)cur->pid,0,0}; u32 hv=0; if(!lenva||ucopy_from(CP,&hv,lenva,4)) return RET_ERR(EFAULT); if(hv>12) hv=12; if(val&&ucopy_to(CP,val,uc,hv)) return RET_ERR(EFAULT); ucopy_to(CP,lenva,&hv,4); return 0; }   /* SO_PEERCRED */ else if(opt==30) v=0; }
    u32 have=0; if(!lenva||ucopy_from(CP,&have,lenva,4)) return RET_ERR(EFAULT);
    if(have>=4){ if(ucopy_to(CP,val,&v,4)) return RET_ERR(EFAULT); have=4; ucopy_to(CP,lenva,&have,4); }
    return 0;
}
static u64 sys_shutdown(u64 fdn,u64 how){ int s=fd_sock(fdn); if(s<0) return RET_ERR(fdn<MAXFD&&CP->fd[fdn].type?ENOTSOCK:EBADF); int r=sock_shutdown(s,(int)how); return r<0?RET_ERR(-r):0; }

/* ---------- descriptores en mensajes AF_UNIX (SCM_RIGHTS) ---------- */
#define SCM_MAX_FD 253
typedef struct { u64 len; int level, type; } Cmsg;
typedef struct {
    SockRights link;
    int count;
    FD fds[];
} UnixRights;

void sock_rights_release(SockRights *link){
    if(!link) return;
    UnixRights *rights=(UnixRights*)link;
    for(int i=0;i<rights->count;i++) fd_close(&rights->fds[i]);
    kfree(rights);
}

static int scm_from_user(u64 addr,u64 size,UnixRights **out){
    int fds[SCM_MAX_FD], count=0;
    for(u64 pos=0;pos<size&&size-pos>=sizeof(Cmsg);){
        Cmsg header;
        if(ucopy_from(CP,&header,addr+pos,sizeof header)) return -EFAULT;
        if(header.len<sizeof header||header.len>size-pos) return -EINVAL;
        if(header.level==1&&header.type==1){
            u64 n=(header.len-sizeof header)/sizeof(int);
            if(n>(u64)(SCM_MAX_FD-count)) return -EINVAL;
            if(n&&ucopy_from(CP,fds+count,addr+pos+sizeof header,n*sizeof(int))) return -EFAULT;
            count+=(int)n;
        }
        u64 advance=(header.len+7)&~7ull;
        if(advance<header.len||advance>size-pos) break;
        pos+=advance;
    }
    if(!count) return 0;
    for(int i=0;i<count;i++)
        if(fds[i]<0||fds[i]>=MAXFD||!CP->fd[fds[i]].type) return -EBADF;
    UnixRights *rights=kmalloc(sizeof *rights+(size_t)count*sizeof(FD));
    if(!rights) return -ENOMEM;
    rights->count=count;
    for(int i=0;i<count;i++) fd_dup_into(&rights->fds[i],&CP->fd[fds[i]]);
    *out=rights;
    return 0;
}

static int scm_to_user(SockRights *link,u64 addr,u64 size,u64 msgva,u64 flags){
    struct { Cmsg header; int fds[SCM_MAX_FD]; } control={0};
    UnixRights *rights=(UnixRights*)link;
    int count=rights?rights->count:0, installed=0;
    u64 limit=addr&&size>=sizeof(Cmsg)?(size-sizeof(Cmsg))/sizeof(int):0;
    while(installed<count&&(u64)installed<limit){
        int fd=fd_alloc(CP,0);
        if(fd<0) break;
        fd_dup_into(&CP->fd[fd],&rights->fds[installed]);
        CP->fd[fd].cloexec=(flags&MSG_CMSG_CLOEXEC)!=0;
        control.fds[installed++]=fd;
    }

    u64 used=0;
    if(installed){
        control.header=(Cmsg){sizeof(Cmsg)+(u64)installed*sizeof(int),1,1};
        used=(control.header.len+7)&~7ull;
        if(used>size) used=size;
    }
    u32 result_flags=installed<count?MSG_CTRUNC:0;
    if((used&&ucopy_to(CP,addr,&control,control.header.len))||
       ucopy_to(CP,msgva+40,&used,8)||ucopy_to(CP,msgva+48,&result_flags,4)){
        for(int i=0;i<installed;i++) fd_close(&CP->fd[control.fds[i]]);
        return -EFAULT;
    }
    return 0;
}

static u64 sys_sendmsg(u64 fdn,u64 msgva,u64 flags){
    int s=fd_sock(fdn);
    if(s<0) return RET_ERR(fdn<MAXFD&&CP->fd[fdn].type?ENOTSOCK:EBADF);
    u64 m[7], vec[32][2], total=0;
    if(ucopy_from(CP,m,msgva,sizeof m)) return RET_ERR(EFAULT);
    if(m[3]>32) return RET_ERR(EINVAL);
    for(u64 i=0;i<m[3];i++){
        if(ucopy_from(CP,vec[i],m[2]+i*16,16)) return RET_ERR(EFAULT);
        if(vec[i][1]>sizeof netbuf-total) vec[i][1]=sizeof netbuf-total;
        total+=vec[i][1];
    }

    u32 ip=0; u16 port=0; char up[108]; int pl=0, fam=0;
    int have=m[0]&&(u32)m[1];
    if(have){
        int err=get_sockaddr(m[0],(u32)m[1],&fam,&ip,&port,up,&pl);
        if(err<0) return RET_ERR(-err);
    }
    UnixRights *rights=0;
    if(m[4]&&m[5]&&sock_family(s)==SK_AF_UNIX){
        int err=scm_from_user(m[4],m[5],&rights);
        if(err<0) return RET_ERR(-err);
    }
    FD *f=&CP->fd[fdn];
    int nb=(f->flags&0x800)||(flags&MSG_DONTWAIT);
    u32 timeout=f->tmo_s;
    int result;
    sock_ref(s);
    for(;;){
        if(fd_sock(fdn)!=s){ result=-EBADF; break; }
        u64 pos=0;
        for(u64 i=0;i<m[3];i++){
            if(vec[i][1]&&ucopy_from(CP,netbuf+pos,vec[i][0],vec[i][1])) break;
            pos+=vec[i][1];
        }
        if(pos!=total){ result=-EFAULT; break; }
        result=sock_send(s,netbuf,(int)total,ip,port,up,pl,have,rights?&rights->link:0);
        if(result!=-EAGAIN||nb) break;
        if(INTR()){ result=-EINTR; break; }
        int wait=wait_fd((int)fdn,POUT,timeout);
        if(wait){ result=wait==1?-EINTR:-EAGAIN; break; }
    }
    if(rights&&(result<0||(result==0&&sock_type(s)!=SK_DGRAM))) sock_rights_release(&rights->link);
    sock_unref(s);
    if(result==-EPIPE&&!(flags&MSG_NOSIGNAL)) sig_self(13);
    return result<0?RET_ERR(-result):(u64)result;
}

static u64 sys_recvmsg(u64 fdn,u64 msgva,u64 flags){
    int s=fd_sock(fdn);
    if(s<0) return RET_ERR(fdn<MAXFD&&CP->fd[fdn].type?ENOTSOCK:EBADF);
    u64 m[7], vec[32][2], cap=0;
    if(ucopy_from(CP,m,msgva,sizeof m)) return RET_ERR(EFAULT);
    if(m[3]>32) return RET_ERR(EINVAL);
    for(u64 i=0;i<m[3];i++){
        if(ucopy_from(CP,vec[i],m[2]+i*16,16)) return RET_ERR(EFAULT);
        if(vec[i][1]>sizeof netbuf-cap) vec[i][1]=sizeof netbuf-cap;
        cap+=vec[i][1];
    }
    FD *f=&CP->fd[fdn];
    int nb=(f->flags&0x800)||(flags&MSG_DONTWAIT), peek=(flags&MSG_PEEK)!=0;
    u32 timeout=f->tmo_r;
    int r;
    SockRights *rights=0;
    u32 ip=0; u16 port=0;
    sock_ref(s);
    for(;;){
        if(fd_sock(fdn)!=s){ r=-EBADF; break; }
        r=sock_recv(s,netbuf,(int)cap,peek,&ip,&port,&rights);
        if(r!=-EAGAIN||nb) break;
        if(INTR()){ r=-EINTR; break; }
        int wait=wait_fd((int)fdn,PIN,timeout);
        if(wait){ r=wait==1?-EINTR:-EAGAIN; break; }
    }
    if(r<0){ sock_unref(s); return RET_ERR(-r); }
    int err=0;
    for(u64 i=0,pos=0;i<m[3]&&pos<(u64)r;i++){
        u64 n=vec[i][1];
        if(n>(u64)r-pos) n=(u64)r-pos;
        if(n&&ucopy_to(CP,vec[i][0],netbuf+pos,n)){ err=-EFAULT; break; }
        pos+=n;
    }
    if(!err&&m[0]&&sock_family(s)==SK_AF_INET) err=put_sockaddr(m[0],msgva+8,2,ip,port,"",0);
    if(!err) err=scm_to_user(rights,m[4],m[5],msgva,flags);
    if(!peek) sock_rights_release(rights);
    sock_unref(s);
    return err<0?RET_ERR(-err):(u64)r;
}

/* ---------- eventfd / timerfd ---------- */
static KObj *obj_new(int kind){ KObj *o=kmalloc(sizeof *o); if(o){ memset(o,0,sizeof *o); o->refs=1; o->kind=kind; } return o; }
static void tfd_update(KObj *o){
    if(!o->t_next) return;
    u32 now=kticks();
    if((int)(now-(u32)o->t_next)<0) return;
    o->cnt++;
    if(o->t_int){ u64 extra=(u64)(u32)(now-(u32)o->t_next)/o->t_int; o->cnt+=extra; o->t_next=(u64)(u32)((u32)o->t_next+(u32)((extra+1)*o->t_int)); if(!o->t_next) o->t_next=1; }
    else o->t_next=0;
}
static u64 sys_eventfd(u64 init,u64 flags){
    int fd=fd_alloc(CP,0); if(fd<0) return RET_ERR(EMFILE);
    KObj *o=obj_new(6); if(!o) return RET_ERR(ENOMEM); o->cnt=init; o->flags=(int)(flags&1);
    FD *f=&CP->fd[fd]; memset(f,0,sizeof *f); f->type=6; f->obj=o; f->flags=(flags&0x800)?0x800:0; f->cloexec=(flags&0x80000)?1:0; return (u64)fd;
}
static u64 sys_timerfd_create(u64 clk,u64 flags){
    (void)clk; int fd=fd_alloc(CP,0); if(fd<0) return RET_ERR(EMFILE);
    KObj *o=obj_new(8); if(!o) return RET_ERR(ENOMEM);
    FD *f=&CP->fd[fd]; memset(f,0,sizeof *f); f->type=8; f->obj=o; f->flags=(flags&0x800)?0x800:0; f->cloexec=(flags&0x80000)?1:0; return (u64)fd;
}
static u64 sys_timerfd_settime(u64 fdn,u64 flags,u64 newv,u64 oldv){
    if(fdn>=MAXFD||CP->fd[fdn].type!=8) return RET_ERR(EBADF);
    KObj *o=CP->fd[fdn].obj;
    tfd_update(o);
    if(oldv){ u64 v[4]; u32 now=kticks(); u64 left=o->t_next?(u64)(u32)((u32)o->t_next-now):0; v[0]=o->t_int/100; v[1]=(o->t_int%100)*10000000ull; v[2]=left/100; v[3]=(left%100)*10000000ull; if(ucopy_to(CP,oldv,v,32)) return RET_ERR(EFAULT); }
    u64 v[4]; if(ucopy_from(CP,v,newv,32)) return RET_ERR(EFAULT);
    u64 val=v[2]*100+v[3]/10000000ull, itv=v[0]*100+v[1]/10000000ull; if((v[2]||v[3])&&!val) val=1; if((v[0]||v[1])&&!itv) itv=1;
    o->cnt=0; o->t_int=itv;
    if(!(v[2]||v[3])) o->t_next=0; else { o->t_next=(flags&1)?val:(u64)(u32)(kticks()+val); if(!o->t_next) o->t_next=1; }
    return 0;
}
static u64 sys_timerfd_gettime(u64 fdn,u64 curv){
    if(fdn>=MAXFD||CP->fd[fdn].type!=8) return RET_ERR(EBADF);
    KObj *o=CP->fd[fdn].obj;
    tfd_update(o);
    u32 now=kticks(); u64 left=o->t_next?(u64)(u32)((u32)o->t_next-now):0; u64 v[4]={o->t_int/100,(o->t_int%100)*10000000ull,left/100,(left%100)*10000000ull};
    return ucopy_to(CP,curv,v,32)?RET_ERR(EFAULT):0;
}

/* ---------- epoll ---------- */
static u64 sys_epoll_create(u64 flags){
    int fd=fd_alloc(CP,0); if(fd<0) return RET_ERR(EMFILE);
    KObj *o=obj_new(7); if(!o) return RET_ERR(ENOMEM);
    FD *f=&CP->fd[fd]; memset(f,0,sizeof *f); f->type=7; f->obj=o; f->cloexec=(flags&0x80000)?1:0; return (u64)fd;
}
static u64 sys_epoll_ctl(u64 epfd,u64 op,u64 fdn,u64 evva){
    if(epfd>=MAXFD||CP->fd[epfd].type!=7) return RET_ERR(EBADF);
    if(fdn>=MAXFD||!CP->fd[fdn].type||fdn==epfd) return RET_ERR(EBADF);
    KObj *o=CP->fd[epfd].obj; u8 ev[12]; int i;
    for(i=0;i<o->nw;i++) if(o->w[i].fd==(int)fdn) break;
    if(op==2){ if(i==o->nw) return RET_ERR(ENOENT); o->w[i]=o->w[--o->nw]; return 0; }
    if(ucopy_from(CP,ev,evva,12)) return RET_ERR(EFAULT);
    u32 e=*(u32*)ev; u64 d=*(u64*)(ev+4);
    if(op==1){ if(i<o->nw) return RET_ERR(EEXIST); if(o->nw>=EP_MAX) return RET_ERR(ENOMEM); o->w[o->nw].fd=(int)fdn; o->w[o->nw].ev=e; o->w[o->nw].data=d; o->nw++; return 0; }
    if(op==3){ if(i==o->nw) return RET_ERR(ENOENT); o->w[i].ev=e; o->w[i].data=d; return 0; }
    return RET_ERR(EINVAL);
}
/* cuenta (y si 'out' no es 0, escribe) los eventos listos; EPOLLET se trata como nivel (seguro: solo hay avisos de mas) */
static int epoll_scan(Proc *pr,KObj *o,u64 out,int max,int report){
    int n=0;
    for(int i=0;i<o->nw&&n<max;i++){
        u32 we=o->w[i].ev; if(!(we&0x3FFFFFFFu)) continue;      /* oneshot ya disparado: sin eventos hasta un MOD */
        int ev=fd_events_p(pr,o->w[i].fd); if(ev&PNVAL) continue;
        u32 rev=(u32)ev&(we|PERR|PHUP); if(!rev) continue;
        if(report){ u8 e[12]; *(u32*)e=rev; *(u64*)(e+4)=o->w[i].data; if(ucopy_to(pr,out+(u64)n*12,e,12)) return -1; if(we&0x40000000u) o->w[i].ev=we&0xC0000000u; }
        n++;
    }
    return n;
}
static int w_epoll(Task *t){ FD *f=&t->pr->fd[t->wa_fd]; if(f->type!=7) return 1; return epoll_scan(t->pr,f->obj,0,64,0)>0; }
static u64 sys_epoll_wait(u64 epfd,u64 out,u64 max,u64 tmo_ms,u64 sigmask,u64 sigsize){
    if(epfd>=MAXFD||CP->fd[epfd].type!=7) return RET_ERR(EBADF);
    max=(u64)(long)(int)max; tmo_ms=(u64)(long)(int)tmo_ms;       /* int de 32 bits: -1 llega como 0xffffffff */
    if((long)max<=0||max>1024) return RET_ERR(EINVAL);
    int m=(int)max; if(m>64) m=64;
    if(sigmask&&sigsize==8){ u64 nm; if(ucopy_from(CP,&nm,sigmask,8)) return RET_ERR(EFAULT); cur->sigsaved=cur->sigmask; cur->has_saved=1; cur->sigmask=nm&~UNBLOCKABLE; }
    KObj *o=CP->fd[epfd].obj;
    int n=epoll_scan(CP,o,out,m,1); if(n<0) return RET_ERR(EFAULT);
    if(n>0||tmo_ms==0) return (u64)n;
    if(INTR()) return RET_ERR(EINTR);
    u32 tk=(long)tmo_ms<0?0:(u32)((tmo_ms+9)/10); if((long)tmo_ms>=0&&!tk) tk=1;
    cur->wa_fd=(int)epfd; int b=block_fn(w_epoll,tk);
    if(b==1) return RET_ERR(EINTR);
    if(CP->fd[epfd].type!=7) return RET_ERR(EBADF);
    n=epoll_scan(CP,o,out,m,1); if(n<0) return RET_ERR(EFAULT);
    return (u64)n;
}

/* ---------- poll / select ---------- */
static int poll_scan(Proc *pr,u64 va,int n,int write){
    int cnt=0;
    for(int i=0;i<n;i++){ u8 e[8]; if(ucopy_from(pr,e,va+(u64)i*8,8)) return -1;
        int fdn=*(int*)e; u16 want=*(u16*)(e+4); u16 rev=0;
        if(fdn>=0){ int ev=fd_events_p(pr,fdn); rev=(u16)(ev&(want|PERR|PHUP|PNVAL)); }
        if(write){ *(u16*)(e+6)=rev; if(ucopy_to(pr,va+(u64)i*8,e,8)) return -1; }
        if(rev) cnt++; }
    return cnt;
}
static int w_poll(Task *t){ return poll_scan(t->pr,t->wa_p1,t->wa_n,0)!=0; }
static u64 do_poll(u64 va,u64 n,long tmo_ms,u64 sigmask,u64 sigsize){
    if(n>1024) return RET_ERR(EINVAL);
    if(sigmask&&sigsize==8){ u64 nm; if(ucopy_from(CP,&nm,sigmask,8)) return RET_ERR(EFAULT); cur->sigsaved=cur->sigmask; cur->has_saved=1; cur->sigmask=nm&~UNBLOCKABLE; }
    int c=poll_scan(CP,va,(int)n,1); if(c<0) return RET_ERR(EFAULT);
    if(c>0||tmo_ms==0) return (u64)c;
    if(INTR()) return RET_ERR(EINTR);
    u32 tk=tmo_ms<0?0:(u32)((tmo_ms+9)/10); if(tmo_ms>0&&!tk) tk=1;
    cur->wa_p1=va; cur->wa_n=(int)n; int b=block_fn(w_poll,tk);
    if(b==1) return RET_ERR(EINTR);
    c=poll_scan(CP,va,(int)n,1); return c<0?RET_ERR(EFAULT):(u64)c;
}
static int select_scan(Proc *pr,int nfds,u64 rva,u64 wva,u64 eva,int write){
    u64 rs[16],ws[16],es[16],ro[16],wo[16]; int bytes=((nfds+63)/64)*8, cnt=0;
    memset(rs,0,sizeof rs); memset(ws,0,sizeof ws); memset(es,0,sizeof es); memset(ro,0,sizeof ro); memset(wo,0,sizeof wo);
    if(rva&&ucopy_from(pr,rs,rva,(u64)bytes)) return -1;
    if(wva&&ucopy_from(pr,ws,wva,(u64)bytes)) return -1;
    if(eva&&ucopy_from(pr,es,eva,(u64)bytes)) return -1;
    for(int f=0;f<nfds;f++){ u64 bit=1ull<<(f&63); int w=f>>6; int wr=(rs[w]&bit)!=0, ww=(ws[w]&bit)!=0; if(!wr&&!ww) continue;
        int ev=fd_events_p(pr,f); if(ev&PNVAL) return -2;
        if(wr&&(ev&(PIN|PHUP|PERR))){ ro[w]|=bit; cnt++; } if(ww&&(ev&(POUT|PERR))){ wo[w]|=bit; cnt++; } }
    if(write){ if(rva&&ucopy_to(pr,rva,ro,(u64)bytes)) return -1; if(wva&&ucopy_to(pr,wva,wo,(u64)bytes)) return -1; if(eva){ u64 z[16]; memset(z,0,sizeof z); if(ucopy_to(pr,eva,z,(u64)bytes)) return -1; } }
    return cnt;
}
static int w_select(Task *t){ int r=select_scan(t->pr,t->wa_n,t->wa_p1,t->wa_p2,t->wa_p3,0); return r!=0; }
static u64 do_select(u64 nfds,u64 rva,u64 wva,u64 eva,long tmo_ticks,u64 sigmask){
    if(nfds>1024) return RET_ERR(EINVAL);
    if(sigmask){ u64 sm[2]; if(!ucopy_from(CP,sm,sigmask,16)&&sm[0]){ u64 nm; if(ucopy_from(CP,&nm,sm[0],8)) return RET_ERR(EFAULT); cur->sigsaved=cur->sigmask; cur->has_saved=1; cur->sigmask=nm&~UNBLOCKABLE; } }
    int c=select_scan(CP,(int)nfds,rva,wva,eva,0); if(c==-2) return RET_ERR(EBADF); if(c<0) return RET_ERR(EFAULT);      /* primero solo se mira: escribir los conjuntos (vacios) antes de esperar los borraria */
    if(c>0||tmo_ticks==0){ if(select_scan(CP,(int)nfds,rva,wva,eva,1)<0) return RET_ERR(EFAULT); return (u64)c; }
    if(INTR()) return RET_ERR(EINTR);
    cur->wa_n=(int)nfds; cur->wa_p1=rva; cur->wa_p2=wva; cur->wa_p3=eva;
    int b=block_fn(w_select,tmo_ticks<0?0:(u32)tmo_ticks); if(b==1) return RET_ERR(EINTR);
    c=select_scan(CP,(int)nfds,rva,wva,eva,1); return c<0?RET_ERR(EFAULT):(u64)c;
}

/* ---------- dispositivos de /dev ---------- */
/* /dev/input/event0: cola de eventos de entrada (formato evdev de Linux) que llena la ventana "Pantalla X" del escritorio */
struct InEv { long sec, usec; u16 type, code; int value; };
#define EVQ_N 512
static struct InEv evq[EVQ_N]; static int evq_h, evq_t;
void kinput_event(int type,int code,int value){
    int n=(evq_h+1)%EVQ_N; if(n==evq_t) return;                      /* cola llena: se descarta */
    u32 k=kticks(); evq[evq_h].sec=k/100; evq[evq_h].usec=(long)(k%100)*10000; evq[evq_h].type=(u16)type; evq[evq_h].code=(u16)code; evq[evq_h].value=value; evq_h=n;
}
static int evq_avail(void){ return evq_h!=evq_t; }
static int w_evq(Task *t){ (void)t; return evq_avail(); }
static int dev_events(int dev){ if(dev==DEV_EVENT) return POUT|(evq_avail()?PIN:0); return PIN|POUT; }
static u64 console_read(u64 buf,u64 n,int nb);
static u64 dev_read(FD *f,u64 buf,u64 n){
    switch(f->aux){
    case DEV_NULL: return 0;
    case DEV_ZERO: case DEV_FULL: { static u8 z[4096]; u64 d=0; while(d<n){ u64 c=n-d>sizeof z?sizeof z:n-d; if(ucopy_to(CP,buf+d,z,c)) return d?d:RET_ERR(EFAULT); d+=c; } return n; }
    case DEV_RANDOM: { u8 r[256]; u64 d=0; while(d<n){ u64 c=n-d>sizeof r?sizeof r:n-d; rnd_fill(r,c); if(ucopy_to(CP,buf+d,r,c)) return d?d:RET_ERR(EFAULT); d+=c; } return n; }
    case DEV_TTY: return console_read(buf,n,(f->flags&0x800)!=0);
    case DEV_EVENT: {
        if(n<sizeof(struct InEv)) return RET_ERR(EINVAL);
        while(!evq_avail()){ if(f->flags&0x800) return RET_ERR(EAGAIN); if(block_fn(w_evq,0)==1) return RET_ERR(EINTR); }
        u64 d=0; while(d+sizeof(struct InEv)<=n&&evq_avail()){ if(ucopy_to(CP,buf+d,&evq[evq_t],sizeof(struct InEv))) return d?d:RET_ERR(EFAULT); evq_t=(evq_t+1)%EVQ_N; d+=sizeof(struct InEv); }
        return d; }
    }
    return RET_ERR(ENODEV);
}
static u64 dev_write(FD *f,u64 buf,u64 n){
    u8 tmp[1024]; u64 done=0;
    switch(f->aux){
    case DEV_NULL: case DEV_ZERO: case DEV_RANDOM: return n;
    case DEV_FULL: return RET_ERR(28);
    case DEV_EVENT: return n;
    case DEV_TTY: while(done<n){ u64 c=n-done>sizeof tmp?sizeof tmp:n-done; if(ucopy_from(CP,tmp,buf+done,c)) return done?done:RET_ERR(EFAULT); k_write_console((char*)tmp,(int)c); done+=c; } return n;
    }
    return RET_ERR(ENODEV);
}
static void dev_create(void){
    static const struct { const char *n; int d; } L[]={{"null",DEV_NULL},{"zero",DEV_ZERO},{"urandom",DEV_RANDOM},{"random",DEV_RANDOM},{"tty",DEV_TTY},{"console",DEV_TTY},{"full",DEV_FULL}};
    FsNode *root=fs_root(); if(!root) return;
    const char *dirs[]={"dev","tmp","proc","sys","run","var","etc","usr","bin","lib","lib64","root","home","opt","mnt"};
    for(unsigned i=0;i<sizeof dirs/sizeof *dirs;i++){ FsNode *d=fs_find(root,dirs[i]); if(!d){ d=fs_mkdir(root,dirs[i]); if(d) d->sys=1; } }
    FsNode *dev=fs_find(root,"dev"); if(!dev) return;
    for(unsigned i=0;i<sizeof L/sizeof *L;i++) if(!fs_find(dev,L[i].n)){ FsNode *n=fs_create(dev,L[i].n,"",0); if(n){ n->sys=1; n->dev=L[i].d; } }
    const char *sub[]={"shm","input","dri"}; for(unsigned i=0;i<3;i++) if(!fs_find(dev,sub[i])){ FsNode *d=fs_mkdir(dev,sub[i]); if(d) d->sys=1; }
    { FsNode *in=fs_find(dev,"input"); if(in&&!fs_find(in,"event0")){ FsNode *n=fs_create(in,"event0","",0); if(n){ n->sys=1; n->dev=DEV_EVENT; } } }
    FsNode *tmp=fs_find(root,"tmp"); if(tmp&&!fs_find(tmp,".X11-unix")){ FsNode *d=fs_mkdir(tmp,".X11-unix"); if(d) d->sys=1; }
}

static int fd_events_p(Proc *pr,int fdn){
    if(fdn<0||fdn>=MAXFD||!pr->fd[fdn].type) return PNVAL;
    FD *f=&pr->fd[fdn];
    switch(f->type){
    case 1: stdin_pump(); return POUT|(tty_input_ready()?PIN:0);
    case 3: { struct Pipe *p=f->pipe; int e=0; if(p->count>0) e|=PIN; if(p->writers<=0) e|=PHUP; return e; }
    case 4: { struct Pipe *p=f->pipe; int e=0; if(p->count<4096) e|=POUT; if(p->readers<=0) e|=PERR; return e; }
    case 5: return sock_events(f->aux);
    case 6: return (f->obj->cnt>0?PIN:0)|(f->obj->cnt<0xFFFFFFFFFFFFFFFEull?POUT:0);
    case 7: return epoll_scan(pr,f->obj,0,64,0)>0?PIN:0;
    case 8: tfd_update(f->obj); return f->obj->cnt?PIN:0;
    case 10: if(f->aux==DEV_TTY){ stdin_pump(); return POUT|(tty_input_ready()?PIN:0); } return dev_events(f->aux);
    default: return PIN|POUT;
    }
}
