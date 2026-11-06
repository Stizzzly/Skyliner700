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
// Exercise the same pump used by the native loop, including simultaneous wakes.
struct PumpHost {
    bool event,frame,quit;int order,waits;
    PumpHost():event(false),frame(false),quit(false),order(0),waits(0){}
    void Wait(){++waits;}
    bool EventReady(){return event;}
    void ReadEventL(){order=order*10+1;event=false;}
    void DispatchReadyL(){order=order*10+2;}
    bool Quitting(){return quit;}
    bool FrameReady(){return frame;}
    void DrawFrameL(){order=order*10+3;frame=false;}
};
struct CompletionHost {
    AuxDispatchGate aux;
    int signals,objects,walks,runs,blocking,wakeObjects;
    bool flight,event,quit;
    CompletionHost():signals(0),objects(0),walks(0),runs(0),blocking(0),wakeObjects(0),flight(true),event(false),quit(false){}
    int RequestCount()const{return signals;}
    void WaitForAnyRequest(){if(signals)--signals;else{++blocking;objects+=wakeObjects;}}
    bool EventReady()const{return event;}
    bool ContinuousFrames()const{return flight;}
    bool Quitting()const{return quit;}
    bool RunAuxRequestL(){++walks;if(!objects)return false;--objects;++runs;return true;}
    void Tick(){WaitGameRequests(*this,aux);DispatchGameReadyL(*this,aux);}
};
static void CheckCompletionProtocol(){
    CompletionHost h;h.Tick();int initial=h.walks;
    for(int i=0;i<1000;++i)h.Tick();
    Check(h.walks==initial&&!h.blocking,"quiet flight neither traverses AOs nor sleeps");
    h.signals=h.objects=100;
    h.Tick();Check(h.runs==32&&h.signals==36,"completion and AO batches are independently bounded");
    h.Tick();h.Tick();h.Tick();
    Check(h.runs==100&&!h.objects&&!h.signals,"ready AOs survive signals drained in earlier frames");
    int walks=h.walks;h.Tick();Check(h.walks==walks,"exhausted batch stops traversal");
    h.flight=false;h.wakeObjects=3;h.Tick();
    Check(h.blocking==1&&h.runs==103,"blocking wait preserves consumed wake for AO dispatch");
    h.flight=true;h.wakeObjects=0;
    WaitGameRequests(h,h.aux);++h.signals;++h.objects;
    DispatchGameReadyL(h,h.aux);
    Check(h.objects==1,"late non-WS completion stays queued");h.Tick();
    Check(!h.objects&&h.runs==104,"next wait dispatches late completion without losing its signal");
    WaitGameRequests(h,h.aux);h.event=true;h.objects=2;
    DispatchGameReadyL(h,h.aux);h.event=false;
    Check(!h.objects&&h.runs==106,"late ready WS event also enables AO service");
    h.aux.Observe(true);h.quit=true;h.objects=1;walks=h.walks;
    DispatchGameReadyL(h,h.aux);
    Check(h.walks==walks&&h.objects==1,"exit does not start more auxiliary callbacks");
    puts("PASS: shared EKA1 pump protocol: quiet flight, bounded bursts, consumed blocking wake, late completions, WS wake and exit.");
}
int main(){
    CheckCompletionProtocol();
    PumpHost pump;pump.event=true;pump.frame=true;PumpGameRequestsL(pump);
    Check(pump.waits==1&&pump.order==123&&!pump.event&&!pump.frame,"owned pump handles input before auxiliary requests and frame");
    pump.order=0;PumpGameRequestsL(pump);Check(pump.order==2,"spurious completion never reads a pending event or timer");
    pump.order=0;pump.event=true;pump.frame=true;pump.quit=true;PumpGameRequestsL(pump);
    Check(pump.order==2&&pump.frame,"exit skips queued input and rendering before teardown");
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
