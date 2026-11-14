/* Conversión local del RTC UTC, con rollover de fecha y año.
 * America/Lima usa UTC-5 sin cambio horario estacional.
 * Utilizable sin libc en los kernels de 32/64 bits. */
#ifndef VORTEX_CLOCK_CALENDAR_H
#define VORTEX_CLOCK_CALENDAR_H
typedef struct {
    int year,month,day,hour,minute;
} VortexCalendar;
static int vortex_leap(int y){return y%4==0&&(y%100!=0||y%400==0);}
static int vortex_month_days(int y,int mo){
    static const unsigned char days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    if(mo<1||mo>12)return 0;
    return days[mo-1]+(mo==2&&vortex_leap(y));
}
static int vortex_calendar_shift(int year,int month,int day,int hour,int minute,
                                  int offset_hours,VortexCalendar *out){
    if(!out||year<1970||year>9999||month<1||month>12||
       day<1||day>vortex_month_days(year,month)||
       hour<0||hour>23||minute<0||minute>59||
       offset_hours< -12||offset_hours>14)return -1;
    hour+=offset_hours;
    if(hour<0){
        hour+=24;day--;
        if(day<1){
            month--;if(month<1){month=12;year--;}
            day=vortex_month_days(year,month);
        }
    }else if(hour>=24){
        hour-=24;day++;
        if(day>vortex_month_days(year,month)){
            day=1;month++;if(month>12){month=1;year++;}
        }
    }
    out->year=year;out->month=month;out->day=day;
    out->hour=hour;out->minute=minute;
    return 0;
}
#endif
