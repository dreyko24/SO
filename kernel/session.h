#ifndef VORTEX_SESSION_H
#define VORTEX_SESSION_H

enum SessionLoad { SESSION_NONE=0, SESSION_READY=1, SESSION_CORRUPT=-1 };
enum SessionResult {
    SESSION_OK=0, SESSION_INVALID=-1, SESSION_DENIED=-2,
    SESSION_EXISTS=-3, SESSION_FULL=-4, SESSION_SAVE_ERROR=-5
};
enum {
    SESSION_MAX_USERS=8, SESSION_NAME_MAX=24,
    SESSION_PASSWORD_MIN=8, SESSION_PASSWORD_MAX=64
};

/* Cargar después de montar VortexFS. Cierra cualquier sesión previa. */
enum SessionLoad session_load(void);
int session_login(const char *name,const char *password);
/* La primera cuenta es administradora y abre sesión. Las demás requieren
 * una sesión administradora. Sin VortexFS montado, los cambios viven en RAM. */
int session_create(const char *name,const char *password);
void session_logout(void);
int session_change_password(const char *old_password,const char *new_password);
const char *session_user(void);
const char *session_name(unsigned index);
unsigned session_count(void);
int session_is_admin(void);

#endif
