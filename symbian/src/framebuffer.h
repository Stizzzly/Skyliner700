#ifndef SKY_FRAMEBUFFER_H
#define SKY_FRAMEBUFFER_H
#include "soft3d.h"
#include "hud.h"
#include <string.h>
// Two output rows can share each source load; all screen accesses are writes.
inline unsigned SkyPresentRead(const Pixel* p){
#if defined(__MARM__) && defined(__GCC32__)
    unsigned value;__asm__("ldrh %0, [%1]":"=r"(value):"r"(p),"m"(*p));return value;
#else
    return *p;
#endif
}
inline unsigned SkyPresentWord(const Pixel* p){
#if defined(__MARM__) && defined(__GCC32__)
    unsigned value;__asm__("ldr %0, [%1]":"=r"(value):"r"(p),"m"(p[0]),"m"(p[1]));return value;
#else
    unsigned value;memcpy(&value,p,sizeof(value));return value;
#endif
}
inline void SkyPresentPair(Pixel* p,unsigned value){
#if defined(__MARM__) && defined(__GCC32__)
    __asm__ __volatile__("str %0, [%1]" : :"r"(value),"r"(p):"memory");
#else
    memcpy(p,&value,sizeof(value));
#endif
}
inline void SkyScaleRows(const Pixel* source,Pixel* first,Pixel* second,int left,int right){
    int x=left;
    if(x&1){Pixel p=(Pixel)SkyPresentRead(source+(x>>1));first[x]=p;if(second)second[x]=p;++x;}
    if((((size_t)(first+x))&3)==0&&(!second||(((size_t)(second+x))&3)==0)){
        if(x+1<right&&(((size_t)(source+(x>>1)))&3)!=0){
            unsigned p=SkyPresentRead(source+(x>>1)),pair=p|(p<<16);
            SkyPresentPair(first+x,pair);if(second)SkyPresentPair(second+x,pair);x+=2;
        }
        for(;x+3<right;x+=4){
            unsigned pixels=SkyPresentWord(source+(x>>1));
            unsigned low=(pixels&65535u)|(pixels<<16),high=(pixels>>16)|(pixels&0xffff0000u);
            SkyPresentPair(first+x,low);SkyPresentPair(first+x+2,high);
            if(second){SkyPresentPair(second+x,low);SkyPresentPair(second+x+2,high);}
        }
        for(;x+1<right;x+=2){unsigned p=SkyPresentRead(source+(x>>1)),pair=p|(p<<16);
            SkyPresentPair(first+x,pair);if(second)SkyPresentPair(second+x,pair);
        }
    }
    for(;x<right;++x){Pixel p=(Pixel)SkyPresentRead(source+(x>>1));first[x]=p;if(second)second[x]=p;}
}
inline void SkyScaleRect(const Pixel* source,int sourceStride,Pixel* screen,int stride,
                         int left,int top,int right,int bottom){
    if(left>=right||top>=bottom)return;
    int y=top;
    if(y&1){SkyScaleRows(source+(y>>1)*sourceStride,screen+y*stride,0,left,right);++y;}
    for(;y+1<bottom;y+=2)SkyScaleRows(source+(y>>1)*sourceStride,screen+y*stride,screen+(y+1)*stride,left,right);
    if(y<bottom)SkyScaleRows(source+(y>>1)*sourceStride,screen+y*stride,0,left,right);
}
// Scale only the visible scene outside the opaque HUD panel.
inline void SkyPresentFlightRect(const Pixel* source,int sourceStride,Pixel* screen,int stride,
                                 int left,int top,int right,int bottom){
    if(left<0)left=0;
    if(top<0)top=0;
    if(right>SKY_DISPLAY_W)right=SKY_DISPLAY_W;
    if(bottom>SKY_DISPLAY_H)bottom=SKY_DISPLAY_H;
    if(left>=right||top>=bottom)return;
    int upper=bottom<SKY_HUD_TOP?bottom:SKY_HUD_TOP;
    SkyScaleRect(source,sourceStride,screen,stride,left,top,right,upper);
    int middleTop=top>SKY_HUD_TOP?top:SKY_HUD_TOP;
    int middleBottom=bottom<SKY_HUD_BOTTOM?bottom:SKY_HUD_BOTTOM;
    int leftEnd=right<SKY_HUD_LEFT?right:SKY_HUD_LEFT;
    int rightStart=left>SKY_HUD_RIGHT?left:SKY_HUD_RIGHT;
    SkyScaleRect(source,sourceStride,screen,stride,left,middleTop,leftEnd,middleBottom);
    SkyScaleRect(source,sourceStride,screen,stride,rightStart,middleTop,right,middleBottom);
    int lower=top>SKY_HUD_BOTTOM?top:SKY_HUD_BOTTOM;
    SkyScaleRect(source,sourceStride,screen,stride,left,lower,right,bottom);
}
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
