#include <assert.h>
#include <stdio.h>
#include "../kernel/clock_calendar.h"
static void check(int year,int month,int day,int hour,int tz,
                  int ey,int em,int ed,int eh){
    assert(vortex_weekday(2026,11,14)==5);
    assert(vortex_weekday(2024,2,29)==3);
    assert(vortex_weekday(2026,11,31)<0);
    VortexCalendar c={0};
    assert(!vortex_calendar_shift(year,month,day,hour,42,tz,&c));
    assert(c.year==ey&&c.month==em&&c.day==ed&&c.hour==eh&&c.minute==42);
}
int main(void){
    check(2026,11,14,3,-5,2026,11,13,22);
    check(2026,1,1,2,-5,2025,12,31,21);
    check(2024,3,1,1,-5,2024,2,29,20);
    check(2025,3,1,1,-5,2025,2,28,20);
    check(2025,12,31,23,3,2026,1,1,2);
    check(2026,11,14,15,-5,2026,11,14,10);
    VortexCalendar c={0};
    assert(vortex_calendar_shift(2025,2,29,12,0,-5,&c));
    assert(vortex_calendar_shift(2026,11,14,25,0,-5,&c));
    assert(vortex_calendar_shift(2026,11,14,4,0,-13,&c));
    puts("VORTEX RTC: UTC -> AMERICA/LIMA, ANO NUEVO Y BISIESTO OK");
    return 0;
}
