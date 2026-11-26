/* La consola de rescate también comprueba las cuentas persistentes. */
static void console_account_read(char *out,int limit,int hidden){
    int n=0;
    for(;;){
        usb_xhci_service();
        int c=getc_nb();
        if(c<0){__asm__ volatile("sti;hlt");continue;}
        if(c=='\n'){out[n]=0;putc('\n');return;}
        if(c=='\b'||c==0x7f){
            if(n){n--;out[n]=0;if(!hidden)putc('\b');}
        }else if(c>=32&&c<=255&&n<limit){
            out[n++]=(char)c;
            if(!hidden)putc((char)c);
        }
    }
}
static void console_accounts(int show,int mount_error){
    enum SessionLoad loaded=session_load();
    if(mount_error||loaded==SESSION_CORRUPT){
        kprintf("Cuentas no disponibles. Reinicia con una copia valida de VortexFS.\n");
        for(;;)__asm__ volatile("sti;hlt");
    }
    if(loaded==SESSION_NONE&&!show)return;
    for(;;){
        char name[SESSION_NAME_MAX+1]={0};
        char secrets[2][SESSION_PASSWORD_MAX+1]={{0}};
        kprintf(loaded==SESSION_NONE?"Crear cuenta local\nUsuario: ":"Iniciar sesion\nUsuario: ");
        console_account_read(name,SESSION_NAME_MAX,0);
        kprintf("Contrasena: ");
        console_account_read(secrets[0],SESSION_PASSWORD_MAX,1);
        int result;
        if(loaded==SESSION_NONE){
            kprintf("Confirmar contrasena: ");
            console_account_read(secrets[1],SESSION_PASSWORD_MAX,1);
            result=k_strcmp(secrets[0],secrets[1])?SESSION_INVALID:session_create(name,secrets[0]);
        }else result=session_login(name,secrets[0]);
        volatile char *clear=(volatile char *)secrets;
        for(size_t i=0;i<sizeof secrets;i++)clear[i]=0;
        if(result==SESSION_OK){kprintf("[vortex-session] consola desbloqueada\n");return;}
        if(result==SESSION_SAVE_ERROR)kprintf("No se pudo guardar la cuenta en VortexFS.\n");
        else kprintf(loaded==SESSION_NONE?"Datos invalidos. Usa al menos 8 caracteres.\n":
                                             "Usuario o contrasena incorrectos.\n");
        uint32_t retry=kticks()+100;
        while((int32_t)(kticks()-retry)<0)__asm__ volatile("sti;hlt");
    }
}
