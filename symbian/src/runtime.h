#ifndef SKY_RUNTIME_H
#define SKY_RUNTIME_H
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
