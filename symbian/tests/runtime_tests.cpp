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
    FlightButtons keys;
    keys.Change(FlightButtons::RollLeft,true);
    keys.Change(FlightButtons::PitchUp,true);
    FlightButtons::Snapshot input=keys.Take();
    Check(input.held==(FlightButtons::RollLeft|FlightButtons::PitchUp)&&input.pressed==input.held&&!input.released,"simultaneous controls survive a frame snapshot");
    for(int i=0;i<1000;++i)keys.Change(FlightButtons::RollLeft,true);
    input=keys.Take();
    Check(!input.pressed&&!input.released&&input.held==(FlightButtons::RollLeft|FlightButtons::PitchUp),"repeat downs do not retrigger actions");
    keys.Change(FlightButtons::RollLeft,false);
    keys.Change(FlightButtons::Select,true);keys.Change(FlightButtons::Select,false);
    input=keys.Take();
    Check(input.held==FlightButtons::PitchUp&&input.pressed==FlightButtons::Select&&input.released==(FlightButtons::RollLeft|FlightButtons::Select),"tap between frames preserves down and up edges");
    keys.Change(FlightButtons::Exit,true);keys.Clear();input=keys.Take();
    Check(!input.held&&!input.pressed&&!input.released,"focus loss clears held and queued commands");
    keys.Change(FlightButtons::Select,true);keys.Take();
    input=keys.Take();Check(!input.pressed&&input.held==FlightButtons::Select,"holding select across menu transition cannot toggle again");
    keys.Clear();
    keys.ChangeScan(53,FlightButtons::Select,true);keys.Take();
    keys.ChangeScan(165,FlightButtons::Select,true);
    keys.ChangeScan(53,FlightButtons::Select,false);input=keys.Take();
    Check(input.held==FlightButtons::Select&&!input.pressed&&!input.released,"select aliases do not release or retrigger each other");
    keys.ChangeScan(165,FlightButtons::Select,false);input=keys.Take();
    Check(!input.held&&input.released==FlightButtons::Select,"last physical select release clears action");
    keys.ChangeScan(14,FlightButtons::RollLeft,true);keys.Take();
    for(int i=0;i<1000;++i)keys.ChangeScan(14,FlightButtons::RollLeft,true);
    input=keys.Take();Check(input.held==FlightButtons::RollLeft&&!input.pressed,"physical repeat counts stay bounded");
    keys.Clear();keys.ChangeScan(14,FlightButtons::RollLeft,false);input=keys.Take();
    Check(!input.held&&!input.released,"late up after focus loss is harmless");
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
    puts("PASS: button snapshots, taps, combinations, repeat/alias handling and focus reset; repeat policy and FPS counting.");return 0;
}
