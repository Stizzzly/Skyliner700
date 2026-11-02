#ifndef SKY_FRAMEBUFFER_H
#define SKY_FRAMEBUFFER_H
#include "soft3d.h"
#include <string.h>
// Destination starts at the window origin, stride is the physical screen pitch.
// Rectangles are exclusive, window-relative, already restricted by DSA.
inline void SkyPresentRect(const Pixel* source,int sourceStride,Pixel* screen,int stride,
                           int left,int top,int right,int bottom,bool scale){
    if(left<0)left=0;
    if(top<0)top=0;
    if(right>SKY_DISPLAY_W)right=SKY_DISPLAY_W;
    if(bottom>SKY_DISPLAY_H)bottom=SKY_DISPLAY_H;
    if(left>=right||top>=bottom)return;
    if(scale&&left==0&&top==0&&right==SKY_DISPLAY_W&&bottom==SKY_DISPLAY_H){
        SkyScale2x(source,sourceStride,screen,stride);return;
    }
    for(int y=top;y<bottom;++y){
        Pixel* out=screen+y*stride;
        if(scale){
            const Pixel* row=source+(y>>1)*sourceStride;
            for(int x=left;x<right;++x)out[x]=row[x>>1];
        }else memcpy(out+left,source+y*sourceStride+left,(right-left)*sizeof(Pixel));
    }
}
#endif
