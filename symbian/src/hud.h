#ifndef SKY_HUD_INCLUDED
#define SKY_HUD_INCLUDED
#include "soft3d.h"
enum { SKY_HUD_LEFT=2,SKY_HUD_TOP=2,SKY_HUD_RIGHT=174,SKY_HUD_BOTTOM=31,
       SKY_HUD_W=SKY_HUD_RIGHT-SKY_HUD_LEFT,SKY_HUD_H=SKY_HUD_BOTTOM-SKY_HUD_TOP };
class HudState {
public:
    HudState():valid(false),speed(0),altitude(0),throttle(0),fps(0){line1[0]=line2[0]=0;}
    bool Update(int newSpeed,int newAltitude,int newThrottle,int newFps);
    const Pixel* Panel()const{return panel;}
    const char* FirstLine()const{return line1;}
    const char* SecondLine()const{return line2;}
private:
    bool valid;int speed,altitude,throttle,fps;
    char line1[40],line2[40];Pixel panel[SKY_HUD_W*SKY_HUD_H];
};
// Exclusive window-relative clip, already restricted by the DSA drawing region.
void SkyPaintHudRect(const HudState& hud,Pixel* screen,int stride,int left,int top,int right,int bottom);
#endif
