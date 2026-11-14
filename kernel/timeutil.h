#ifndef VORTEX_TIMEUTIL_H
#define VORTEX_TIMEUTIL_H
/* Conversion gregoriana pura, comprobable en host. days=0 corresponde a 1970-01-01 UTC.
 * Solo usa enteros de 32 bits: no necesita libgcc en el kernel x86. */
static inline void vortex_date_from_days(int days,int *year,int *month,int *day){
    int z=days+719468;
    int era=(z>=0?z:z-146096)/146097;
    int doe=z-era*146097;
    int yoe=(doe-doe/1460+doe/36524-doe/146096)/365;
    int y=yoe+era*400;
    int doy=doe-(365*yoe+yoe/4-yoe/100);
    int mp=(5*doy+2)/153;
    int d=doy-(153*mp+2)/5+1;
    int m=mp+(mp<10?3:-9);
    y+=m<=2;
    *year=y; *month=m; *day=d;
}
#endif
