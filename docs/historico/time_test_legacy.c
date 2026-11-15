/* Pruebas host sin dispositivos: conversion de fechas y UTC-5 America/Lima. */
#include <stdint.h>
#include <stdio.h>
#include "../kernel/timeutil.h"
static int errors;
static void check(int days,int y,int m,int d){
    int a,b,c; vortex_date_from_days(days,&a,&b,&c);
    if(a!=y||b!=m||c!=d){
        fprintf(stderr,"dias=%d: %04d-%02d-%02d, esperado %04d-%02d-%02d\n",days,a,b,c,y,m,d);
        errors++;
    }
}
int main(void){
    check(0,1970,1,1);
    check(11016,2000,2,29);
    check(19782,2024,2,29);
    check(20771,2026,11,14);
    check(20770,2026,11,13);
    /* 14 nov 2026 02:00 UTC = 13 nov 2026 21:00 Peru */
    int64_t utc=(int64_t)20771*86400+2*3600;
    int64_t peru=utc-5*3600;
    int days=(int)(peru/86400),secs=(int)(peru%86400);
    check(days,2026,11,13);
    if(secs!=21*3600){ fprintf(stderr,"hora Lima incorrecta: %d\n",secs); errors++; }
    if(errors) return 1;
    puts("FECHA Y ZONA AMERICA/LIMA OK");
    return 0;
}
