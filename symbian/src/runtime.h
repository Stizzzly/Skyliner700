#ifndef SKY_RUNTIME_H
#define SKY_RUNTIME_H
// WS down/up edges accumulate until the frame takes one coherent snapshot.
// A down+up between frames preserves both edges; typematic downs add none.
class FlightButtons {
public:
    enum Button {PitchUp=1,PitchDown=2,RollLeft=4,RollRight=8,
        ThrottleUp=16,ThrottleDown=32,YawRight=64,YawLeft=128,
        Select=256,Exit=512,Reset=1024,MenuUp=2048,MenuDown=4096};
    struct Snapshot {unsigned held,pressed,released;};
    FlightButtons(){Clear();}
    void ChangeScan(unsigned scan,unsigned mask,bool down){
        if(scan>=256||!mask)return;
        if(down==bool(scans[scan]))return;
        scans[scan]=down?1:0;
        // Several physical keys can select the same action (5/joystick/softkey).
        // Releasing one must not release another that is still held.
        unsigned bit=mask;int index=0;
        while((bit>>1)!=0){bit>>=1;++index;}
        if(index>=13)return;
        if(down){if(++counts[index]==1)Change(mask,true);}
        else if(counts[index]&&--counts[index]==0)Change(mask,false);
    }
    void Change(unsigned mask,bool down){
        if(down){pressed|=mask&~held;held|=mask;}
        else {released|=mask&held;held&=~mask;}
    }
    Snapshot Take(){Snapshot s={held,pressed,released};pressed=0;released=0;return s;}
    void Clear(){held=0;pressed=0;released=0;for(int i=0;i<256;++i)scans[i]=0;for(int j=0;j<13;++j)counts[j]=0;}
private:
    unsigned held,pressed,released;
    unsigned char scans[256],counts[13];
};
// Session/interval template permits testing the actual lifecycle without WSERV.
template<class Session,class Interval> class KeyRepeatGuard {
    bool saved;Interval initial,period;
public:
    KeyRepeatGuard():saved(false),initial(0),period(0){}
    void Update(bool active,Session& session){
        if(active&&!saved){
            session.GetKeyboardRepeatRate(initial,period);
            // These are durations, not booleans. Avoid undefined zero semantics.
            Interval delayed(2147483647);
            session.SetKeyboardRepeatRate(delayed,delayed);saved=true;
        }else if(!active)Restore(session);
    }
    void Restore(Session& session){
        if(saved){session.SetKeyboardRepeatRate(initial,period);saved=false;}
    }
};
// Count frame intervals over real time; avoid integer-EMA bias near 30 FPS.
class FrameRateCounter {
    int elapsed,frames,value;
public:
    FrameRateCounter():elapsed(0),frames(0),value(0){}
    void Reset(){elapsed=0;frames=0;}
    int Add(int microseconds){
        if(microseconds<=0||microseconds>2000000){Reset();return value;}
        elapsed+=microseconds;++frames;
        if(elapsed>=1000000||frames>=1000){
            value=(frames*1000000+elapsed/2)/elapsed;Reset();
        }
        return value;
    }
};
#endif
