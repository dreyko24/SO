#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../kernel/session.h"
#include "../kernel/fs.h"
#include "../kernel/disk.h"

static int mounted, save_fails;
static uint8_t checkpoint[16384];
static int checkpoint_size;
static uint8_t clean_checkpoint[16384];
static int clean_size;

void *kmalloc(size_t n){ return malloc(n); }
void kfree(void *p){ free(p); }
void kprintf(const char *format,...){ (void)format; }
uint32_t kticks(void){ return 100; }
uint64_t krtc_epoch(void){ return 1770000000; }
int vortexfs_active(void){ return mounted; }
int vortexfs_save(void){
    if(save_fails)return -1;
    checkpoint_size=fs_serialize(checkpoint,sizeof checkpoint);
    assert(checkpoint_size>0);
    return 0;
}

static const uint8_t *account_file(int *bytes){
    const uint8_t *data;
    assert(fs_accounts_read(&data,bytes)>=0);
    return data;
}

static int contains(const uint8_t *data,int bytes,const char *value){
    size_t len=strlen(value);
    for(size_t i=0;i+len<=(size_t)bytes;i++)
        if(!memcmp(data+i,value,len))return 1;
    return 0;
}

static void overlay_account(unsigned type,const uint8_t *data,int bytes){
    uint8_t snapshot[512]={2,7,'S','i','s','t','e','m','a',0,11,'c','u','e','n','t','a','s','.','v','s','a'};
    snapshot[9]=(uint8_t)type;
    int pos=22, count=type==1?4:2;
    assert(bytes>=0&&bytes<=(int)sizeof snapshot-pos-count-2);
    for(int i=0;i<count;i++)snapshot[pos++]=(uint8_t)((unsigned)bytes>>(8*i));
    memcpy(snapshot+pos,data,(size_t)bytes);
    assert(!fs_overlay(snapshot,pos+bytes+2));
}

static void protected_path(void){
    FsNode *root=fs_root(), *system=fs_find(root,"Sistema");
    assert(system && !fs_create(system,"cuentas.vsa","replacement",11));
    assert(!fs_mkdir(system,"cuentas.vsa"));
    assert(!fs_symlink(system,"cuentas.vsa","version.txt"));
    assert(fs_rename(system,"Sistema-anterior") && fs_move(system,root,"Sistema-anterior"));
    assert(fs_remove(system));
    FsNode *file=fs_create(system,"test-publico","ok",2);
    assert(file && fs_rename(file,"cuentas.vsa") && fs_move(file,system,"cuentas.vsa"));
    assert(!fs_copy(file,system,"cuentas.vsa") && !fs_link(file,system,"cuentas.vsa"));
    assert(!fs_write(file,"yes",3) && !fs_remove(file));
    assert(!fs_find(system,"cuentas.vsa") && !fs_resolve(root,"/Sistema/cuentas.vsa"));
    FsNode *alias=fs_symlink(root,"test-alias","/Sistema/cuentas.vsa");
    assert(alias && !fs_resolve(root,"/test-alias") && !fs_remove(alias));
    FsNode *copy=fs_copy(system,root,"Sistema-copia");
    assert(copy && !fs_find(copy,"cuentas.vsa") && !fs_remove(copy));
}

static void accounts(void){
    const char *old="First-pass-2026", *next="New-password-2026";
    assert(session_load()==SESSION_NONE);
    assert(session_login("admin","admin")!=SESSION_OK);
    assert(session_create("bad/name",old)==SESSION_INVALID);
    assert(session_create("admin","short")==SESSION_INVALID);

    mounted=1;
    save_fails=1;
    assert(session_create("Admin",old)==SESSION_SAVE_ERROR);
    int size;
    assert(!account_file(&size) && !size && !session_user() && !session_count());
    save_fails=0;
    assert(session_create("Admin",old)==SESSION_OK);
    assert(session_is_admin() && !strcmp(session_user(),"Admin"));
    assert(session_create("admin",old)==SESSION_EXISTS);

    assert(session_create("Visitante",old)==SESSION_OK);
    protected_path();
    const uint8_t *file=account_file(&size);
    assert(file && size>0 && !contains(file,size,old));
    /* Header 24, records 74, salt at byte 26 of each record. */
    assert(memcmp(file+50,file+124,16));
    assert(!strcmp(session_name(1),"Visitante") && session_count()==2);
    session_logout();
    assert(!session_user() && !session_is_admin());
    assert(session_login("Visitante","incorrect-password")==SESSION_DENIED);
    assert(session_login("visitante",old)==SESSION_OK);
    assert(!session_is_admin() && session_create("other",old)==SESSION_DENIED);
    assert(session_change_password("incorrect-password",next)==SESSION_DENIED);

    uint8_t before[256];
    assert(size<=(int)sizeof before);
    memcpy(before,file,(size_t)size);
    save_fails=1;
    assert(session_change_password(old,next)==SESSION_SAVE_ERROR);
    int after;
    assert(file==account_file(&after) && after==size && !memcmp(file,before,(size_t)size));
    save_fails=0;
    assert(session_change_password(old,next)==SESSION_OK);
    file=account_file(&size);
    assert(memcmp(file+124,before+124,16));
    assert(!contains(file,size,next));

    assert(!fs_deserialize(checkpoint,checkpoint_size));
    assert(session_load()==SESSION_READY && !session_user());
    protected_path();
    assert(session_login("Visitante",old)==SESSION_DENIED);
    assert(session_login("Visitante",next)==SESSION_OK);
    assert(session_login("Admin",old)==SESSION_OK && session_is_admin());

    file=account_file(&size);
    assert(size<=(int)sizeof before);
    memcpy(before,file,(size_t)size);
    before[size-1]^=1;
    overlay_account(1,before,size);
    assert(session_load()==SESSION_CORRUPT);
    assert(!session_user() && !session_count());
    assert(session_create("other",old)==SESSION_DENIED);
    assert(session_login("Admin",old)==SESSION_DENIED);
}

static void known_password_vector(void){
    /* Fixture calculado con Python hashlib PBKDF2-HMAC-SHA256, 600.000 rondas. */
    static const uint8_t vector[]={
        0x56,0x54,0x58,0x41,0x43,0x43,0x54,0x01,0xc0,0x27,0x09,0x00,
        0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x76,0x65,0x63,0x74,0x6f,0x72,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x01,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,
        0x0b,0x0c,0x0d,0x0e,0x0f,0x10,0xf0,0x7b,0x4c,0x17,0x2a,0x64,
        0x42,0x07,0x14,0x1f,0x75,0xd1,0xa6,0xf5,0xd8,0x61,0x0c,0xca,
        0xfb,0xc4,0x99,0xdf,0x71,0x5c,0x2d,0xf7,0x86,0x74,0x84,0xf8,
        0x09,0x11,0xeb,0xb2,0xa0,0xde,0x17,0x34,0x07,0xf7,0xff,0x72,
        0x6e,0xe1,0x11,0x89,0x0a,0xa8,0xa1,0x3e,0xe9,0xfa,0x66,0x23,
        0x75,0xbe,0x23,0x72,0x5a,0x62,0x76,0x31,0xd8,0xbe,
    };
    overlay_account(1,vector,sizeof vector);
    assert(session_load()==SESSION_READY);
    assert(session_login("vector","Vector-pass-2026")==SESSION_OK);
    assert(session_is_admin());
    static const uint8_t target[]="version.txt";
    overlay_account(3,target,sizeof target-1);
    assert(session_load()==SESSION_CORRUPT);
    assert(!vortexfs_save() && !fs_deserialize(checkpoint,checkpoint_size));
    assert(session_load()==SESSION_CORRUPT);
}

int main(void){
    fs_init();
    protected_path();
    clean_size=fs_serialize(clean_checkpoint,sizeof clean_checkpoint);
    assert(clean_size>0);
    accounts();
    known_password_vector();
    mounted=0;
    save_fails=1;
    assert(!fs_deserialize(clean_checkpoint,clean_size));
    assert(session_load()==SESSION_NONE);
    assert(session_create("live","Temporary-password")==SESSION_OK && session_is_admin());
    puts("VORTEX SESSION OK");
    return 0;
}
