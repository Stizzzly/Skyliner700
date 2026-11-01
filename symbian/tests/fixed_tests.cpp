#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "game/flight.h"
#include "world/terrain.h"
#include "reference.inc"
static void Check(bool ok,const char* text){if(!ok){fprintf(stderr,"FAIL: %s\n",text);exit(1);}}
static Fix From(double x){return Fix::Raw((int)(x*65536+(x>=0?0.5:-0.5)));}
static double Real(Fix x){return x.raw/65536.0;}
static double Error(Fix x,double y){return fabs(Real(x)-y);}
int main(){
    Check(sizeof(Fix)==4,"32-bit representation");
    for(int raw=-1000000;raw<=1000000;raw+=17){
        Fix angle=Fix::Raw(raw);
        Check(FxTan(angle).raw==(FxSin(angle)/FxCos(angle)).raw,"tangent table matches old quantized sin/cos exactly");
    }
    double sineError=0,atanError=0,sqrtError=0;
    for(int i=-12000;i<=12000;++i){
        Fix angle=From(i/1000.0);double e=Error(FxSin(angle),sin(Real(angle)));if(e>sineError)sineError=e;
        Fix x=From((i%99)/17.0),y=From(i/1000.0);
        e=Error(FxAtan2(y,x),atan2(Real(y),Real(x)));if(e>atanError)atanError=e;
        Fix value=From((i+12000)/3.0);
        e=Error(FxSqrt(value),sqrt(Real(value)));if(e>sqrtError)sqrtError=e;
    }
    Check(sineError<0.0088,"360-degree nearest-entry sine accuracy");
    Check(atanError<0.00008,"atan quadrants and accuracy");
    Check(sqrtError<0.004,"normalized square-root lookup accuracy");
    Check(FxFloor(From(-0.1))==-1&&FxFloor(From(-1))==-1,"negative floor");
    Check((Fix::Raw(2147483647)+Fix(1)).raw==2147483647,"addition saturates");
    Check((Fix(30000)*Fix(2)).raw==2147483647,"multiplication saturates");
    double maxHeightError=0;
    for(int x=-1000;x<=1000;x+=41)for(int z=-1000;z<=1000;z+=43){
        double e=Error(Terrain_GetHeight(Fix(x),Fix(z)),Reference::Terrain_GetHeight((float)x,(float)z));
        if(e>maxHeightError)maxHeightError=e;
    }
    Check(maxHeightError<0.5,"terrain tracks original height field");
    FlightModel fixed;Reference::FlightModel reference;FlightModel_Reset(&fixed);Reference::FlightModel_Reset(&reference);
    Fix dt=From(0.02);double maxPositionError=0,maxSpeedError=0;
    for(int step=0;step<3000;++step){
        double pitch=step>=1500&&step<1550?0.3:0;
        double roll=step>=1800&&step<1850?0.4:0;
        FlightInput in={0};in.throttle=1;in.pitch=From(pitch);in.roll=From(roll);
        Reference::FlightInput rin={0};rin.throttle=1;rin.pitch=(float)Real(in.pitch);rin.roll=(float)Real(in.roll);
        FlightModel_Step(&fixed,&in,dt);Reference::FlightModel_Step(&reference,&rin,(float)Real(dt));
        double dx=Real(fixed.state.x)-reference.state.x,dy=Real(fixed.state.y)-reference.state.y,dz=Real(fixed.state.z)-reference.state.z;
        double e=sqrt(dx*dx+dy*dy+dz*dz);if(e>maxPositionError)maxPositionError=e;
        e=Error(fixed.state.speed,reference.state.speed);if(e>maxSpeedError)maxSpeedError=e;
        Check(fixed.state.speed<=Fix(91),"bounded airspeed");
    }
    printf("Math max error: sin %.7f, atan %.7f, sqrt %.7f; terrain %.3fm\n",sineError,atanError,sqrtError,maxHeightError);
    printf("60s differential flight: max position %.3fm, speed %.3fm/s\n",maxPositionError,maxSpeedError);
    Check(maxPositionError<15,"60-second trajectory drift");Check(maxSpeedError<1,"speed tracks original model");
    Check(fixed.state.altitude>From(10),"takeoff/climb");
    // High-speed attitudes must still establish opposite vertical motion.
    for(int sign=-1;sign<=1;sign+=2){
        FlightModel_Reset(&fixed);fixed.state.y+=300;fixed.state.onGround=0;
        fixed.velocityZ=-60;fixed.state.pitch=From(sign*0.16);
        Fix start=fixed.state.y;FlightInput in={0};
        for(int step=0;step<75;++step)FlightModel_Step(&fixed,&in,dt);
        Check(sign>0?fixed.state.y>start+2:fixed.state.y<start-2,"climb/descent control");
    }
    FlightModel_Reset(&fixed);Check(fixed.state.onGround&&fixed.state.speed==Fix(0),"reset/contact");
    fixed.state.y+=10;fixed.state.onGround=0;fixed.velocityZ=-30;fixed.state.pitch=From(-0.16);
    bool touched=false;FlightInput neutral={0};
    for(int t=0;t<500;++t){FlightModel_Step(&fixed,&neutral,dt);if(fixed.state.onGround)touched=true;}
    Check(touched,"descending aircraft lands");
    // Left input is negative roll and must turn left relative to the initial
    // heading, including after rotating the aircraft to face another direction.
    for(int heading=0;heading<4;++heading)for(int sign=-1;sign<=1;sign+=2){
        FlightModel_Reset(&fixed);fixed.state.y+=300;fixed.state.onGround=0;
        Fix yaw=From(heading*1.57079632679),rightX=FxCos(yaw),rightZ=FxSin(yaw);
        fixed.state.yaw=yaw;fixed.velocityX=FxSin(yaw)*60;fixed.velocityZ=-FxCos(yaw)*60;
        Fix startX=fixed.state.x,startZ=fixed.state.z;
        FlightInput bank={0};bank.roll=From(sign*0.4);
        for(int t=0;t<100;++t)FlightModel_Step(&fixed,&bank,dt);
        Fix turn=FxWrapAngle(fixed.state.yaw-yaw);
        Fix side=(fixed.state.x-startX)*rightX+(fixed.state.z-startZ)*rightZ;
        Check(sign<0?turn<Fix(0):turn>Fix(0),"joystick left/right heading direction");
        Check(sign<0?side<Fix(0):side>Fix(0),"joystick left/right lateral movement");
        Check(-Fix(sign*4)*FxSin(fixed.state.roll)<Fix(0),"inside wing banks down");
    }
    printf("PASS: fixed arithmetic, math tables, terrain, trajectory, climb/descent and reset.\n");return 0;
}
