#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>
static void Check(bool ok,const char* message){if(!ok){fprintf(stderr,"FAIL: %s\n",message);exit(1);}}
struct Interval {int value;Interval(int v=0):value(v){}};
struct Session {
    int initial,period,gets,sets;
    Session():initial(370000),period(85000),gets(0),sets(0){}
    void GetKeyboardRepeatRate(Interval& a,Interval& b){++gets;a.value=initial;b.value=period;}
    void SetKeyboardRepeatRate(const Interval& a,const Interval& b){++sets;initial=a.value;period=b.value;}
};
int main(){
    Session s;KeyRepeatGuard<Session,Interval> guard;
    for(int i=0;i<100;++i)guard.Update(false,s);
    Check(s.gets==0&&s.sets==0,"menu does not touch system settings");
    guard.Update(true,s);
    Check(s.gets==1&&s.sets==1&&s.initial==2147483647&&s.period==2147483647,"foreground suppresses repeat generation");
    for(int i=0;i<10000;++i)guard.Update(true,s);
    Check(s.gets==1&&s.sets==1,"no WS calls per active frame");
    guard.Update(false,s);guard.Restore(s);
    Check(s.initial==370000&&s.period==85000&&s.sets==2,"pause/focus loss restores exactly once");
    s.initial=410000;s.period=110000;
    guard.Update(true,s);guard.Restore(s);
    Check(s.initial==410000&&s.period==110000,"new foreground session preserves intervening system changes");
    for(int mode=0;mode<3;++mode){
        FrameRateCounter fps;int value=0,us=mode==0?32000:mode==1?33333:40000;
        for(int i=0;i<200;++i)value=fps.Add(us);
        Check(value==(mode==0?31:mode==1?30:25),"FPS meter avoids integer-EMA floor bias");
        fps.Reset();fps.Add(-1);fps.Add(2100000);
        for(int i=0;i<200;++i)value=fps.Add(us);
        Check(value==(mode==0?31:mode==1?30:25),"FPS resumes without pause-time contamination");
    }
    puts("PASS: repeat save/suppress/restore lifecycle, no per-frame WS calls and interval FPS counting.");return 0;
}
