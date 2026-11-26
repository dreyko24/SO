/* Inicio y gestión de la sesión local del escritorio. */
enum AccountPage { ACCOUNT_DESKTOP, ACCOUNT_SETUP, ACCOUNT_LOGIN, ACCOUNT_LOCKED,
                   ACCOUNT_MANAGE, ACCOUNT_ADD, ACCOUNT_CHANGE, ACCOUNT_ERROR };
enum AccountAction { ACCOUNT_SUBMIT, ACCOUNT_GUEST, ACCOUNT_BACK, ACCOUNT_NEW,
                     ACCOUNT_PASSWORD, ACCOUNT_LOCK, ACCOUNT_RESTART };
typedef struct {
    enum AccountPage page;
    int focus, optional;
    uint32_t retry_at;
    char field[3][SESSION_PASSWORD_MAX+1];
    char locked_name[SESSION_NAME_MAX+1];
    const char *notice;
} AccountView;
static AccountView accounts;

static void account_wipe(void){
    volatile unsigned char *p=(volatile unsigned char *)accounts.field;
    for(size_t i=0;i<sizeof accounts.field;i++)p[i]=0;
}
static void account_page(enum AccountPage page,const char *notice){
    account_wipe();
    accounts.page=page;
    accounts.focus=0;
    accounts.retry_at=0;
    accounts.notice=notice;
    dirty=1;
}
int gui_accounts_start(int show){
    enum SessionLoad loaded=session_load();
    if(account_mount_error||loaded==SESSION_CORRUPT){
        account_page(ACCOUNT_ERROR,"No se pudieron leer las cuentas guardadas.");
    }else if(loaded==SESSION_READY){
        account_page(ACCOUNT_LOGIN,0);
        kserial_puts("[vortex-session] cuentas listas\n");
    }else if(show){
        accounts.optional=0;
        account_page(ACCOUNT_SETUP,0);
        kserial_puts("[vortex-session] primera cuenta\n");
    }
    return accounts.page!=ACCOUNT_DESKTOP;
}
static void account_manage(void){
    if(session_user())account_page(ACCOUNT_MANAGE,0);
    else{
        accounts.optional=1;
        account_page(ACCOUNT_SETUP,0);
    }
}
static int account_fields(void){
    switch(accounts.page){
    case ACCOUNT_SETUP: case ACCOUNT_ADD: case ACCOUNT_CHANGE: return 3;
    case ACCOUNT_LOGIN: return 2;
    case ACCOUNT_LOCKED: return 1;
    default: return 0;
    }
}
static const char *account_title(void){
    switch(accounts.page){
    case ACCOUNT_SETUP: return "Crear la primera cuenta";
    case ACCOUNT_LOGIN: return "Iniciar sesión";
    case ACCOUNT_LOCKED: return "Sesión bloqueada";
    case ACCOUNT_MANAGE: return "Usuarios";
    case ACCOUNT_ADD: return "Crear usuario";
    case ACCOUNT_CHANGE: return "Cambiar contraseña";
    default: return "Cuentas no disponibles";
    }
}
static const char *account_field_label(int i){
    static const char *const create[]={"Usuario","Contraseña","Confirmar contraseña"};
    static const char *const change[]={"Contraseña actual","Nueva contraseña","Confirmar contraseña"};
    if(accounts.page==ACCOUNT_LOCKED)return "Contraseña";
    return accounts.page==ACCOUNT_CHANGE?change[i]:create[i];
}
static int account_secret(int i){
    return i>0||accounts.page==ACCOUNT_CHANGE||accounts.page==ACCOUNT_LOCKED;
}
static int account_actions(enum AccountAction action[5]){
    if(accounts.page==ACCOUNT_ERROR){action[0]=ACCOUNT_RESTART;return 1;}
    if(accounts.page==ACCOUNT_MANAGE){
        int n=0;
        if(session_is_admin())action[n++]=ACCOUNT_NEW;
        action[n++]=ACCOUNT_PASSWORD;
        action[n++]=ACCOUNT_LOCK;
        action[n++]=ACCOUNT_RESTART;
        action[n++]=ACCOUNT_BACK;
        return n;
    }
    action[0]=ACCOUNT_SUBMIT;
    if(accounts.page==ACCOUNT_ADD||accounts.page==ACCOUNT_CHANGE||
       (accounts.page==ACCOUNT_SETUP&&accounts.optional)){
        action[1]=ACCOUNT_BACK;
        return 2;
    }
    if(accounts.page==ACCOUNT_SETUP&&!vortexfs_active()){
        action[1]=ACCOUNT_GUEST;
        return 2;
    }
    return 1;
}
static const char *account_action_label(enum AccountAction action){
    switch(action){
    case ACCOUNT_GUEST: return "Entrar como invitado";
    case ACCOUNT_BACK: return "Volver";
    case ACCOUNT_NEW: return "Crear usuario";
    case ACCOUNT_PASSWORD: return "Cambiar contraseña";
    case ACCOUNT_LOCK: return "Bloquear sesión";
    case ACCOUNT_RESTART: return accounts.page==ACCOUNT_ERROR?"Reiniciar":"Reiniciar para cambiar";
    default:
        if(accounts.page==ACCOUNT_LOGIN)return "Iniciar sesión";
        if(accounts.page==ACCOUNT_LOCKED)return "Desbloquear";
        if(accounts.page==ACCOUNT_CHANGE)return "Guardar contraseña";
        return "Crear cuenta";
    }
}
static void account_submit(void){
    enum AccountPage page=accounts.page;
    int login=page==ACCOUNT_LOGIN||page==ACCOUNT_LOCKED, outcome;
    if(login&&(int32_t)(kticks()-accounts.retry_at)<0)return;
    if(account_fields()==3&&k_strcmp(accounts.field[1],accounts.field[2])){
        accounts.notice="Las contraseñas no coinciden.";
        dirty=1;return;
    }
    if(page==ACCOUNT_LOCKED)outcome=session_login(accounts.locked_name,accounts.field[0]);
    else if(page==ACCOUNT_LOGIN)outcome=session_login(accounts.field[0],accounts.field[1]);
    else if(page==ACCOUNT_CHANGE)outcome=session_change_password(accounts.field[0],accounts.field[1]);
    else outcome=session_create(accounts.field[0],accounts.field[1]);
    if(outcome==SESSION_OK){
        if(page==ACCOUNT_ADD||page==ACCOUNT_CHANGE){
            account_page(ACCOUNT_MANAGE,page==ACCOUNT_ADD?"Usuario creado.":"Contraseña actualizada.");
            kserial_puts(page==ACCOUNT_ADD?"[vortex-session] cuenta creada\n":
                                         "[vortex-session] contraseña actualizada\n");
        }else{
            accounts.locked_name[0]=0;
            account_page(ACCOUNT_DESKTOP,0);
            kserial_puts("[vortex-session] escritorio desbloqueado\n");
        }
        return;
    }
    if(login){
        char *secret=accounts.field[page==ACCOUNT_LOCKED?0:1];
        volatile char *clear=secret;
        for(int i=0;i<=SESSION_PASSWORD_MAX;i++)clear[i]=0;
        accounts.retry_at=kticks()+100;
        kserial_puts("[vortex-session] credenciales rechazadas\n");
    }
    accounts.notice=outcome==SESSION_DENIED?"Usuario o contraseña incorrectos.":
        outcome==SESSION_EXISTS?"Ese usuario ya existe.":
        outcome==SESSION_FULL?"Se alcanzó el límite de cuentas.":
        outcome==SESSION_SAVE_ERROR?"No se pudo guardar la cuenta en VortexFS.":
        "Comprueba el usuario y usa al menos 8 caracteres en la contraseña.";
    dirty=1;
}
static void account_activate(enum AccountAction action){
    switch(action){
    case ACCOUNT_SUBMIT: account_submit(); break;
    case ACCOUNT_GUEST:
        if(accounts.page==ACCOUNT_SETUP&&!session_count()&&!vortexfs_active()){
            account_page(ACCOUNT_DESKTOP,0);
            kserial_puts("[vortex-session] invitado en vivo\n");
        }
        break;
    case ACCOUNT_NEW: if(session_is_admin())account_page(ACCOUNT_ADD,0);break;
    case ACCOUNT_PASSWORD: if(session_user())account_page(ACCOUNT_CHANGE,0);break;
    case ACCOUNT_LOCK:
        if(session_user()){
            k_strcpy(accounts.locked_name,session_user(),sizeof accounts.locked_name);
            session_logout();
            account_page(ACCOUNT_LOCKED,0);
            kserial_puts("[vortex-session] escritorio bloqueado\n");
        }
        break;
    case ACCOUNT_RESTART: kreboot();break;
    case ACCOUNT_BACK:
        if(accounts.page==ACCOUNT_ADD||accounts.page==ACCOUNT_CHANGE)account_page(ACCOUNT_MANAGE,0);
        else if(accounts.page==ACCOUNT_MANAGE||accounts.optional)account_page(ACCOUNT_DESKTOP,0);
        break;
    }
}
static void account_key(int c){
    int fields=account_fields();
    enum AccountAction action[5];
    int n=account_actions(action);
    if(c=='\t')accounts.focus=(accounts.focus+1)%(fields+n);
    else if(c==27){
        for(int i=0;i<n;i++)if(action[i]==ACCOUNT_BACK){account_activate(ACCOUNT_BACK);return;}
    }else if(c=='\n'){
        if(accounts.focus>=fields){account_activate(action[accounts.focus-fields]);return;}
        if(accounts.focus+1<fields)accounts.focus++;
        else account_activate(ACCOUNT_SUBMIT);
    }else if(accounts.focus<fields){
        char *field=accounts.field[accounts.focus];
        int len=k_strlen(field);
        int limit=account_secret(accounts.focus)?SESSION_PASSWORD_MAX:SESSION_NAME_MAX;
        if(c=='\b'||c==0x7f){if(len)field[len-1]=0;}
        else if(c>=32&&c<=255&&len<limit){field[len]=(char)c;field[len+1]=0;}
    }
    dirty=1;
}
static void account_bounds(int *x,int *y,int *w,int *h){
    *w=k_min(530,gw-36);*h=k_min(424,gh-20);
    *x=(gw-*w)/2;*y=(gh-*h)/2;
}
static int account_action_y(int y,int h,int count){return y+h-(count>2?138:62);}
static void account_mouse(int px,int py){
    int x,y,w,h;account_bounds(&x,&y,&w,&h);
    int fields=account_fields();
    for(int i=0;i<fields;i++)
        if(in_rect(px,py,x+26,y+111+i*61,w-52,34)){accounts.focus=i;dirty=1;return;}
    enum AccountAction action[5];
    int n=account_actions(action),width=(w-64)/2,top=account_action_y(y,h,n);
    for(int i=0;i<n;i++)
        if(in_rect(px,py,x+26+(i%2)*(width+12),top+(i/2)*38,width,33)){
            accounts.focus=fields+i;account_activate(action[i]);return;
        }
}
static void account_draw(void){
    int x,y,w,h;account_bounds(&x,&y,&w,&h);
    clip_base(0,0,gw,gh);clip_reset();
    fill(0,0,gw,gh,light_theme?0xe8edf6:0x151625,255);
    rrect(x,y,w,h,18,C_BG,255);
    text_big(x+26,y+24,account_title(),C_TXT);
    const char *detail=0;
    switch(accounts.page){
    case ACCOUNT_SETUP: detail=vortexfs_active()?"La primera cuenta administra los usuarios.":
                                         "Modo en vivo. Las cuentas duran hasta apagar.";break;
    case ACCOUNT_LOGIN: detail="Escribe tu usuario y contraseña.";break;
    case ACCOUNT_LOCKED: detail=accounts.locked_name;break;
    case ACCOUNT_MANAGE: detail=session_user();break;
    case ACCOUNT_ADD: detail=vortexfs_active()?"La cuenta se guardará en VortexFS.":"La cuenta se guardará en esta sesión.";break;
    default: break;
    }
    if(detail)text(x+26,y+65,detail,C_DIM);
    int fields=account_fields();
    for(int i=0;i<fields;i++){
        int fy=y+111+i*61;
        text(x+26,fy-20,account_field_label(i),C_DIM);
        rrect(x+26,fy,w-52,34,8,accounts.focus==i?ACCENT:C_BORDER,255);
        rrect(x+27,fy+1,w-54,32,7,C_PANEL,255);
        char mask[SESSION_PASSWORD_MAX+1];
        const char *shown=accounts.field[i];
        if(account_secret(i)){
            int n=k_strlen(shown);
            for(int j=0;j<n;j++)mask[j]='*';
            mask[n]=0;shown=mask;
        }
        while(*shown&&text_w(shown)>w-80)shown++;
        text(x+38,fy+9,shown,C_TXT);
    }
    if(accounts.page==ACCOUNT_MANAGE){
        for(unsigned i=0;i<session_count();i++)
            text(x+26+(int)(i/4)*((w-52)/2),y+97+(int)(i%4)*21,session_name(i),C_TXT);
    }
    if(accounts.notice)
        text(x+26,accounts.page==ACCOUNT_MANAGE?y+193:y+h-104,accounts.notice,
             light_theme?0x803119:0xffbe91);
    enum AccountAction action[5];
    int n=account_actions(action), width=(w-64)/2, top=account_action_y(y,h,n);
    for(int i=0;i<n;i++)
        button(x+26+(i%2)*(width+12),top+(i/2)*38,width,33,account_action_label(action[i]),
               accounts.focus==fields+i);
}
