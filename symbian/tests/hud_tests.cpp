#include "hud.h"
#include "framebuffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
static void Check(bool ok,const char* text){if(!ok){fprintf(stderr,"FAIL: %s\n",text);exit(1);}}
int main(){
    HudState hud;
    Check(hud.Update(123,456,78,51),"initial HUD rebuild");
    Check(strcmp(hud.FirstLine(),"SPD 123   ALT 456")==0,"first line format");
    Check(strcmp(hud.SecondLine(),"THR 78%   FPS 51")==0,"second line format");
    std::vector<Pixel> cached(hud.Panel(),hud.Panel()+SKY_HUD_W*SKY_HUD_H);
    Check(!hud.Update(123,456,78,51),"unchanged values skip formatting/cache rebuild");
    Check(memcmp(&cached[0],hud.Panel(),cached.size()*sizeof(Pixel))==0,"unchanged HUD cache");
    Check(hud.Update(124,456,78,51),"speed change refreshes HUD");
    Check(hud.Update((-2147483647-1),2147483647,-1,0),"extreme signed numbers");
    Check(strcmp(hud.FirstLine(),"SPD -2147483648   ALT 2147483647")==0,"INT_MIN conversion");
    Check(strcmp(hud.SecondLine(),"THR -1%   FPS 0")==0,"sign/percent conversion");
    Check(hud.Update(123,456,78,51),"restore HUD");
    unsigned seed=6600;
    for(int trial=0;trial<1000;++trial){
        int stride=SKY_DISPLAY_W+trial%10,offset=16+(trial&1);
        int sourceStride=SKY_W+trial%7;
        std::vector<Pixel> source(sourceStride*SKY_H),full(stride*SKY_DISPLAY_H+32,0),out(offset+stride*SKY_DISPLAY_H+16,0xA55A);
        for(unsigned i=0;i<source.size();++i){seed=seed*1664525u+1013904223u;source[i]=(Pixel)seed;}
        SkyPresentRect(&source[0],sourceStride,&full[0],stride,0,0,SKY_DISPLAY_W,SKY_DISPLAY_H);
        SkyPaintHudRect(hud,&full[0],stride,0,0,SKY_DISPLAY_W,SKY_DISPLAY_H);
        int l=(int)((seed>>3)%200)-12,t=(int)((seed>>11)%230)-12;
        seed=seed*1664525u+1013904223u;
        int r=(int)((seed>>3)%200)-12,b=(int)((seed>>11)%230)-12;
        if(trial<20){l=t=0;r=SKY_DISPLAY_W;b=SKY_DISPLAY_H;}
        if(trial>=20&&trial<40){l=trial%2?3:1;t=trial%3?3:1;r=175;b=32;}
        SkyPresentFlightRect(&source[0],sourceStride,&out[offset],stride,l,t,r,b);
        for(int y=0;y<SKY_DISPLAY_H;++y)for(int x=0;x<SKY_DISPLAY_W;++x){
            bool clip=x>=l&&x<r&&y>=t&&y<b;
            bool panel=x>=SKY_HUD_LEFT&&x<SKY_HUD_RIGHT&&y>=SKY_HUD_TOP&&y<SKY_HUD_BOTTOM;
            Pixel expected=clip&&!panel?source[y*sourceStride+x]:0xA55A;
            Check(out[offset+y*stride+x]==expected,"scene clips and skips opaque HUD before paint");
        }
        SkyPaintHudRect(hud,&out[offset],stride,l,t,r,b);
        for(unsigned i=0;i<out.size();++i){
            int index=(int)i-offset;bool clip=false;int x=0,y=0;
            if(index>=0&&index<stride*SKY_DISPLAY_H){x=index%stride;y=index/stride;clip=x<SKY_DISPLAY_W&&x>=l&&x<r&&y>=t&&y<b;}
            Pixel expected=clip?full[y*stride+x]:0xA55A;
            Check(out[i]==expected,"HUD composition/clip/stride/outer guards");
        }
    }
    puts("PASS: HUD cache, signed numeric text, 1000 scene/HUD clipped compositions, odd strides/alignment and all guards.");
}
