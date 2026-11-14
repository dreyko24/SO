#include <stdio.h>
#include <string.h>
#include "../kernel/browser_launch.h"
static int errors;
#define CHECK(ok,label) do{if(!(ok)){fprintf(stderr,"BROWSER CMD FAIL: %s\n",label);errors++;}}while(0)
int main(void){
    char cmd[160], small[16];
    CHECK(!vortex_browser_command("https://www.google.com/maps",cmd,sizeof cmd),"HTTPS accepted");
    CHECK(!strcmp(cmd,"abrir vortex-chromium https://www.google.com/maps"),"preserve URL");
    CHECK(!vortex_browser_command("kng://buscar?q=historia+del+peru",cmd,sizeof cmd),"search accepted");
    CHECK(!strcmp(cmd,"abrir vortex-chromium https://www.google.com/search?q=historia+del+peru"),"Google search route");
    CHECK(!vortex_browser_command("kng://inicio",cmd,sizeof cmd),"home route");
    CHECK(!strcmp(cmd,"abrir vortex-chromium https://www.google.com/"),"Google homepage fallback");
    CHECK(vortex_browser_command("https://example.com/;reboot",cmd,sizeof cmd)<0,"reject semicolon");
    CHECK(vortex_browser_command("https://example.com/$(id)",cmd,sizeof cmd)<0,"reject shell substitution");
    CHECK(vortex_browser_command("https://ok",small,sizeof small)<0,"reject insufficient buffer");
    CHECK(vortex_browser_command(0,cmd,sizeof cmd)<0,"reject null URL");
    if(errors) return 1;
    puts("BROWSER CMD: 10/10 checks OK");
    return 0;
}
