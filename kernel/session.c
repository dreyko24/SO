/* Cuentas del escritorio. Guarda verificadores PBKDF2, nunca las claves. */
#include "session.h"
#include "disk.h"
#include "fs.h"
#if defined(__i386__) || defined(__x86_64__)
#include <cpuid.h>
#endif

enum {
    SHA_BYTES=32, SHA_BLOCK=64, SALT_BYTES=16, KDF_ROUNDS=600000,
    HEADER_BYTES=24, NAME_BYTES=SESSION_NAME_MAX+1, RECORD_BYTES=NAME_BYTES+1+SALT_BYTES+SHA_BYTES,
    MAX_FILE_BYTES=HEADER_BYTES+SESSION_MAX_USERS*RECORD_BYTES+SHA_BYTES
};
static const uint8_t magic[8]={'V','T','X','A','C','C','T',1};

typedef struct {
    char name[NAME_BYTES];
    uint8_t salt[SALT_BYTES], verifier[SHA_BYTES];
} Account;

typedef struct {
    uint64_t generation;
    unsigned count;
    Account users[SESSION_MAX_USERS];
} Accounts;

static Accounts accounts;
static enum SessionLoad load_state=SESSION_CORRUPT;
static int signed_in=-1;

static void wipe(void *buffer,size_t size){
    volatile uint8_t *out=buffer;
    while(size--)*out++=0;
}

static int equal_bytes(const uint8_t *left,const uint8_t *right,size_t bytes){
    volatile unsigned diff=0;
    for(size_t i=0;i<bytes;i++)diff|=(unsigned)(left[i]^right[i]);
    return diff==0;
}

/* SHA-256 (FIPS 180-4) y PBKDF2-HMAC-SHA256 (RFC 8018). */
static const uint32_t initial[8]={
    0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
};
static const uint32_t sha_round[64]={
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static uint32_t rotr(uint32_t value,unsigned bits){return (value>>bits)|(value<<(32-bits));}

static void sha_compress(uint32_t state[8],const uint8_t block[SHA_BLOCK]){
    uint32_t words[64];
    for(unsigned i=0;i<16;i++){
        const uint8_t *p=block+4*i;
        words[i]=((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
    }
    for(unsigned i=16;i<64;i++){
        uint32_t a=words[i-15], b=words[i-2];
        uint32_t s0=rotr(a,7)^rotr(a,18)^(a>>3), s1=rotr(b,17)^rotr(b,19)^(b>>10);
        words[i]=words[i-16]+s0+words[i-7]+s1;
    }
    uint32_t a=state[0], b=state[1], c=state[2], d=state[3];
    uint32_t e=state[4], f=state[5], g=state[6], h=state[7];
    for(unsigned i=0;i<64;i++){
        uint32_t s1=rotr(e,6)^rotr(e,11)^rotr(e,25);
        uint32_t choose=(e&f)^(~e&g);
        uint32_t t1=h+s1+choose+sha_round[i]+words[i];
        uint32_t s0=rotr(a,2)^rotr(a,13)^rotr(a,22);
        uint32_t majority=(a&b)^(a&c)^(b&c);
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+s0+majority;
    }
    state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
    state[4]+=e; state[5]+=f; state[6]+=g; state[7]+=h;
}

/* `base` incorpora prefix_bytes bytes, siempre un múltiplo de SHA_BLOCK. */
static void sha_digest(const uint32_t base[8],unsigned prefix_bytes,const uint8_t *data,
                       unsigned bytes,uint8_t out[SHA_BYTES]){
    uint32_t state[8];
    memcpy(state,base,sizeof state);
    uint32_t bits=(prefix_bytes+bytes)*8;
    while(bytes>=SHA_BLOCK){
        sha_compress(state,data);
        data+=SHA_BLOCK;
        bytes-=SHA_BLOCK;
    }
    uint8_t block[SHA_BLOCK]={0};
    memcpy(block,data,bytes);
    block[bytes]=0x80;
    if(bytes>=56){sha_compress(state,block);memset(block,0,sizeof block);}
    for(unsigned i=0;i<4;i++)block[63-i]=(uint8_t)(bits>>(8*i));
    sha_compress(state,block);
    for(unsigned i=0;i<8;i++)for(unsigned j=0;j<4;j++)out[4*i+j]=(uint8_t)(state[i]>>(24-8*j));
}

static void derive(const char *password,unsigned bytes,const uint8_t salt[SALT_BYTES],uint8_t out[SHA_BYTES]){
    uint32_t inner[8], outer[8];
    uint8_t pad[SHA_BLOCK], u[SHA_BYTES], first[SALT_BYTES+4]={0};
    memset(pad,0x36,sizeof pad);
    for(unsigned i=0;i<bytes;i++)pad[i]^=(uint8_t)password[i];
    memcpy(inner,initial,sizeof inner);
    sha_compress(inner,pad);
    for(unsigned i=0;i<SHA_BLOCK;i++)pad[i]^=0x36^0x5c;
    memcpy(outer,initial,sizeof outer);
    sha_compress(outer,pad);
    memcpy(first,salt,SALT_BYTES);
    first[SALT_BYTES+3]=1;
    sha_digest(inner,SHA_BLOCK,first,sizeof first,u);
    sha_digest(outer,SHA_BLOCK,u,sizeof u,u);
    memcpy(out,u,sizeof u);
    for(unsigned round=1;round<KDF_ROUNDS;round++){
        sha_digest(inner,SHA_BLOCK,u,sizeof u,u);
        sha_digest(outer,SHA_BLOCK,u,sizeof u,u);
        for(unsigned i=0;i<SHA_BYTES;i++)out[i]^=u[i];
    }
    wipe(pad,sizeof pad); wipe(inner,sizeof inner); wipe(outer,sizeof outer); wipe(u,sizeof u);
}

static void write_le(uint8_t *out,uint64_t value,unsigned bytes){
    for(unsigned i=0;i<bytes;i++){out[i]=(uint8_t)value;value>>=8;}
}

static uint64_t read_le(const uint8_t *data,unsigned bytes){
    uint64_t value=0;
    while(bytes)value=(value<<8)|data[--bytes];
    return value;
}

static void salt_entropy(uint8_t data[24]){
#if defined(__i386__) || defined(__x86_64__)
    unsigned eax,ebx,ecx,edx;
    if(!__get_cpuid(1,&eax,&ebx,&ecx,&edx))return;
    if(ecx&bit_RDRND){
        for(unsigned i=0;i<4;i++){
            for(unsigned attempt=0;attempt<10;attempt++){
                uint32_t value;
                uint8_t valid;
                __asm__ volatile("rdrand %0; setc %1" : "=r"(value),"=qm"(valid) :: "cc");
                if(valid){write_le(data+4*i,value,4);break;}
            }
        }
    }
    if(edx&(1u<<4)){
        uint32_t low,high;
        __asm__ volatile("rdtsc":"=a"(low),"=d"(high));
        write_le(data+16,low,4);
        write_le(data+20,high,4);
    }
#else
    (void)data;
#endif
}

static void new_salt(uint64_t generation,uint8_t salt[SALT_BYTES]){
    static uint32_t attempts;
    uint8_t data[64]={0}, hash[SHA_BYTES];
    salt_entropy(data);
    write_le(data+24,krtc_epoch(),8);
    write_le(data+32,kticks(),4);
    write_le(data+40,generation,8);
    if(accounts.count)memcpy(data+48,accounts.users[0].salt,SALT_BYTES);
    /* La generación persistida diferencia las altas y cambios de clave aunque
     * una VM no exponga RDRAND. El reloj/TSC no son un RNG criptográfico. */
    for(;;){
        write_le(data+36,++attempts,4);
        sha_digest(initial,0,data,sizeof data,hash);
        unsigned i=0;
        while(i<accounts.count&&!equal_bytes(hash,accounts.users[i].salt,SALT_BYTES))i++;
        if(i==accounts.count)break;
    }
    memcpy(salt,hash,SALT_BYTES);
}

static unsigned username_length(const char *name){
    if(!name)return 0;
    for(unsigned i=0;i<=SESSION_NAME_MAX;i++){
        unsigned char c=(unsigned char)name[i];
        if(!c)return i;
        if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9'))continue;
        if(i&&(c=='_'||c=='-'||c=='.'))continue;
        return 0;
    }
    return 0;
}

static unsigned password_length(const char *password){
    if(!password)return 0;
    for(unsigned i=0;i<=SESSION_PASSWORD_MAX;i++){
        unsigned char c=(unsigned char)password[i];
        if(!c)return i>=SESSION_PASSWORD_MIN?i:0;
        if(c<32||c==127)return 0;
    }
    return 0;
}

static unsigned char ascii_lower(unsigned char c){return c>='A'&&c<='Z'?c+('a'-'A'):c;}

static int find_account(const Accounts *db,const char *name){
    for(unsigned i=0;i<db->count;i++){
        const char *a=db->users[i].name, *b=name;
        while(*a&&ascii_lower(*a)==ascii_lower(*b)){a++;b++;}
        if(!*a&&!*b)return (int)i;
    }
    return -1;
}

static unsigned encode(const Accounts *db,uint8_t out[MAX_FILE_BYTES]){
    memcpy(out,magic,sizeof magic);
    write_le(out+8,KDF_ROUNDS,4);
    write_le(out+12,db->count,4);
    write_le(out+16,db->generation,8);
    unsigned end=HEADER_BYTES;
    for(unsigned i=0;i<db->count;i++,end+=RECORD_BYTES){
        const Account *a=&db->users[i];
        uint8_t *record=out+end;
        memcpy(record,a->name,NAME_BYTES);
        record[NAME_BYTES]=(i==0);
        memcpy(record+NAME_BYTES+1,a->salt,SALT_BYTES);
        memcpy(record+NAME_BYTES+1+SALT_BYTES,a->verifier,SHA_BYTES);
    }
    sha_digest(initial,0,out,end,out+end);
    return end+SHA_BYTES;
}

static int decode(const uint8_t *data,int size,Accounts *db){
    if(!data||size<HEADER_BYTES+RECORD_BYTES+SHA_BYTES||size>MAX_FILE_BYTES)return -1;
    unsigned count=(unsigned)read_le(data+12,4);
    unsigned end=HEADER_BYTES+count*RECORD_BYTES;
    uint64_t generation=read_le(data+16,8);
    if(!count||count>SESSION_MAX_USERS||end+SHA_BYTES!=(unsigned)size||generation<count||
       !equal_bytes(data,magic,sizeof magic)||read_le(data+8,4)!=KDF_ROUNDS)return -1;
    uint8_t hash[SHA_BYTES];
    sha_digest(initial,0,data,end,hash);
    if(!equal_bytes(hash,data+end,sizeof hash))return -1;
    for(unsigned i=0;i<count;i++){
        const uint8_t *record=data+HEADER_BYTES+i*RECORD_BYTES;
        const char *name=(const char *)record;
        unsigned name_bytes=username_length(name);
        if(!name_bytes||find_account(db,name)>=0||record[NAME_BYTES]!=(i==0))return -1;
        for(unsigned j=name_bytes+1;j<NAME_BYTES;j++)if(record[j])return -1;
        Account *a=&db->users[db->count];
        memcpy(a->name,name,NAME_BYTES);
        memcpy(a->salt,record+NAME_BYTES+1,SALT_BYTES);
        for(unsigned j=0;j<db->count;j++)if(equal_bytes(db->users[j].salt,a->salt,SALT_BYTES))return -1;
        memcpy(a->verifier,record+NAME_BYTES+1+SALT_BYTES,SHA_BYTES);
        db->count++;
    }
    db->generation=generation;
    return 0;
}

enum SessionLoad session_load(void){
    signed_in=-1;
    memset(&accounts,0,sizeof accounts);
    load_state=SESSION_CORRUPT;
    const uint8_t *data;
    int bytes, present=fs_accounts_read(&data,&bytes);
    if(present<0)return load_state;
    if(!present)return load_state=SESSION_NONE;
    Accounts loaded={0};
    if(!decode(data,bytes,&loaded)){
        accounts=loaded;
        load_state=SESSION_READY;
    }
    wipe(&loaded,sizeof loaded);
    return load_state;
}

static int store(const Accounts *next){
    const uint8_t *previous;
    int size, present=fs_accounts_read(&previous,&size);
    uint8_t data[MAX_FILE_BYTES];
    if(present<0||present!=!!accounts.count)return -1;
    if(present){
        unsigned bytes=encode(&accounts,data);
        if(!previous||bytes!=(unsigned)size||!equal_bytes(data,previous,bytes))return -1;
    }
    int bytes=(int)encode(next,data);
    return fs_accounts_store(data,bytes,vortexfs_active()?vortexfs_save:0);
}

static int verify(const Account *a,const char *password,unsigned bytes){
    uint8_t result[SHA_BYTES];
    derive(password,bytes,a->salt,result);
    int match=equal_bytes(result,a->verifier,sizeof result);
    wipe(result,sizeof result);
    return match;
}

int session_login(const char *name,const char *password){
    unsigned bytes=password_length(password);
    if(!bytes||!username_length(name)||load_state!=SESSION_READY)return SESSION_DENIED;
    int index=find_account(&accounts,name);
    if(index<0||!verify(&accounts.users[index],password,bytes))return SESSION_DENIED;
    signed_in=index;
    return SESSION_OK;
}

void session_logout(void){signed_in=-1;}
const char *session_user(void){return signed_in>=0?accounts.users[signed_in].name:0;}
const char *session_name(unsigned index){return index<accounts.count?accounts.users[index].name:0;}
unsigned session_count(void){return accounts.count;}
int session_is_admin(void){return signed_in==0;}

static int commit(Accounts *next,Account *user,const char *password,unsigned bytes){
    int result=SESSION_SAVE_ERROR;
    if(next->generation!=UINT64_MAX){
        new_salt(++next->generation,user->salt);
        derive(password,bytes,user->salt,user->verifier);
        if(!store(next)){
            accounts=*next;
            load_state=SESSION_READY;
            result=SESSION_OK;
        }
    }
    wipe(next,sizeof *next);
    return result;
}

int session_create(const char *name,const char *password){
    if(load_state!=SESSION_NONE&&!session_is_admin())return SESSION_DENIED;
    unsigned name_bytes=username_length(name), bytes=password_length(password);
    if(!name_bytes||!bytes)return SESSION_INVALID;
    if(find_account(&accounts,name)>=0)return SESSION_EXISTS;
    if(accounts.count==SESSION_MAX_USERS)return SESSION_FULL;
    Accounts next=accounts;
    unsigned index=next.count++;
    Account *user=&next.users[index];
    memcpy(user->name,name,name_bytes+1);
    int result=commit(&next,user,password,bytes);
    if(result==SESSION_OK&&index==0)signed_in=0;
    return result;
}

int session_change_password(const char *old_password,const char *new_password){
    if(signed_in<0)return SESSION_DENIED;
    unsigned old_bytes=password_length(old_password), bytes=password_length(new_password);
    if(!bytes)return SESSION_INVALID;
    if(!old_bytes||!verify(&accounts.users[signed_in],old_password,old_bytes))return SESSION_DENIED;
    Accounts next=accounts;
    return commit(&next,&next.users[signed_in],new_password,bytes);
}
