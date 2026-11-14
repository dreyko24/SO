#ifndef VORTEX_BROWSER_LAUNCH_H
#define VORTEX_BROWSER_LAUNCH_H
/* Genera argumentos del lanzador de Chromium sin metacaracteres de shell.
 * No confundir con un sistema de seguridad: los argumentos se deben pasar al
 * proceso sin evaluarlos mediante /bin/sh. Solo HTTP(S) o consulta propia. */
static inline int vortex_browser_starts(const char *s,const char *prefix){
    while(*prefix){ if(*s++!=*prefix++) return 0; } return 1;
}
static inline int vortex_browser_safe(unsigned char c){
    return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||
           (c>='0'&&c<='9')||c=='.'||c=='/'||c==':'||
           c=='?'||c=='&'||c=='='||c=='%'||c=='#'||
           c=='_'||c=='-'||c=='+'||c=='~'||c=='@';
}
static inline int vortex_browser_command(const char *url,char *out,int max){
    if(!url||!out||max<=0) return -1;
    out[0]=0;
    const char *command="abrir vortex-chromium ";
    const char *base="", *value=url;
    if(vortex_browser_starts(url,"http://")||vortex_browser_starts(url,"https://")){}
    else if(vortex_browser_starts(url,"kng://buscar?q=")){
        base="https://www.google.com/search?q="; value=url+15;
    } else value="https://www.google.com/";
    int i=0, written=0;
    for(const char *p=command;*p;p++){
        if(i>=max-1) return -1;
        out[i++]=*p;
    }
    for(int section=0;section<2;section++){
        const char *part=section==0?base:value;
        for(;*part;part++){
            if(!vortex_browser_safe((unsigned char)*part)||++written>120||i>=max-1){ out[0]=0; return -1; }
            out[i++]=*part;
        }
    }
    out[i]=0;
    return 0;
}
#endif
