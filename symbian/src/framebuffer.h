#ifndef SKY_FRAMEBUFFER_H
#define SKY_FRAMEBUFFER_H
#include "soft3d.h"
#include "hud.h"
#include <string.h>
// Native 176x208 scene: copy pixels 1:1 inside an exclusive clipped rectangle.
inline void SkyCopyRect(const Pixel* source,int sourceStride,Pixel* screen,int stride,
                        int left,int top,int right,int bottom){
    if(left>=right||top>=bottom)return;
    const Pixel* in=source+top*sourceStride;
    Pixel* out=screen+top*stride;
    for(int y=top;y<bottom;++y,in+=sourceStride,out+=stride)
        memcpy(out+left,in+left,(right-left)*sizeof(Pixel));
}
// Copy only the visible scene outside the opaque HUD panel.
inline void SkyPresentFlightRect(const Pixel* source,int sourceStride,Pixel* screen,int stride,
                                 int left,int top,int right,int bottom){
    if(left<0)left=0;
    if(top<0)top=0;
    if(right>SKY_DISPLAY_W)right=SKY_DISPLAY_W;
    if(bottom>SKY_DISPLAY_H)bottom=SKY_DISPLAY_H;
    if(left>=right||top>=bottom)return;
    int upper=bottom<SKY_HUD_TOP?bottom:SKY_HUD_TOP;
    SkyCopyRect(source,sourceStride,screen,stride,left,top,right,upper);
    int middleTop=top>SKY_HUD_TOP?top:SKY_HUD_TOP;
    int middleBottom=bottom<SKY_HUD_BOTTOM?bottom:SKY_HUD_BOTTOM;
    int leftEnd=right<SKY_HUD_LEFT?right:SKY_HUD_LEFT;
    int rightStart=left>SKY_HUD_RIGHT?left:SKY_HUD_RIGHT;
    SkyCopyRect(source,sourceStride,screen,stride,left,middleTop,leftEnd,middleBottom);
    SkyCopyRect(source,sourceStride,screen,stride,rightStart,middleTop,right,middleBottom);
    int lower=top>SKY_HUD_BOTTOM?top:SKY_HUD_BOTTOM;
    SkyCopyRect(source,sourceStride,screen,stride,left,lower,right,bottom);
}
// Destination starts at the window origin, stride is the physical screen pitch.
// Rectangles are exclusive, window-relative, already restricted by DSA.
inline void SkyPresentRect(const Pixel* source,int sourceStride,Pixel* screen,int stride,
                           int left,int top,int right,int bottom){
    if(left<0)left=0;
    if(top<0)top=0;
    if(right>SKY_DISPLAY_W)right=SKY_DISPLAY_W;
    if(bottom>SKY_DISPLAY_H)bottom=SKY_DISPLAY_H;
    SkyCopyRect(source,sourceStride,screen,stride,left,top,right,bottom);
}
#endif
