#include "hud.h"
#include "ui_font.inc"
#include <string.h>
static Pixel HudColor(int r,int g,int b){return (Pixel)(((r>>3)<<11)|((g>>2)<<5)|(b>>3));}
static char* Append(char* out,const char* text){while(*text)*out++=*text++;return out;}
static char* Number(char* out,int value){
    unsigned n=value<0?0u-(unsigned)value:(unsigned)value;
    if(value<0)*out++='-';
    char reverse[10];int count=0;
    do{reverse[count++]=(char)('0'+n%10);n/=10;}while(n);
    while(count)*out++=reverse[--count];
    return out;
}
static const unsigned char* Glyph(unsigned ch){
    static const unsigned char minus[7]={0,0,0,31,0,0,0};
    static const unsigned char percent[7]={17,18,2,4,8,9,17};
    if(ch>='A'&&ch<='Z')return letters[ch-'A'];
    if(ch>='0'&&ch<='9')return digits[ch-'0'];
    if(ch=='-')return minus;
    if(ch=='%')return percent;
    return 0;
}
static void Text(Pixel* pixels,int stride,int x,int y,const char* text,
                 int left,int top,int right,int bottom,Pixel color){
    for(;*text;++text,x+=6){
        if(x>=right)break;
        const unsigned char* glyph=Glyph((unsigned char)*text);if(!glyph)continue;
        for(int row=0;row<7;++row){int py=y+row;if(py<top||py>=bottom)continue;
            for(int col=0;col<5;++col){int px=x+col;
                if(px>=left&&px<right&&(glyph[row]&(1u<<(4-col))))pixels[py*stride+px]=color;
            }
        }
    }
}
bool HudState::Update(int newSpeed,int newAltitude,int newThrottle,int newFps){
    bool first=!valid||speed!=newSpeed||altitude!=newAltitude;
    bool second=!valid||throttle!=newThrottle||fps!=newFps;
    if(!first&&!second)return false;
    if(first){char* out=Append(line1,"SPD ");out=Number(out,newSpeed);out=Append(out,"   ALT ");out=Number(out,newAltitude);*out=0;}
    if(second){char* out=Append(line2,"THR ");out=Number(out,newThrottle);out=Append(out,"%   FPS ");out=Number(out,newFps);*out=0;}
    speed=newSpeed;altitude=newAltitude;throttle=newThrottle;fps=newFps;valid=true;
    Pixel background=HudColor(7,20,34),foreground=HudColor(220,239,250);
    for(int i=0;i<SKY_HUD_W*SKY_HUD_H;++i)panel[i]=background;
    Text(panel,SKY_HUD_W,4,5,line1,0,0,SKY_HUD_W,SKY_HUD_H,foreground);
    Text(panel,SKY_HUD_W,4,18,line2,0,0,SKY_HUD_W,SKY_HUD_H,foreground);
    return true;
}
void SkyPaintHudRect(const HudState& hud,Pixel* screen,int stride,int left,int top,int right,int bottom){
    if(left<0)left=0;
    if(top<0)top=0;
    if(right>SKY_DISPLAY_W)right=SKY_DISPLAY_W;
    if(bottom>SKY_DISPLAY_H)bottom=SKY_DISPLAY_H;
    if(left>=right||top>=bottom)return;
    int l=left>SKY_HUD_LEFT?left:SKY_HUD_LEFT,t=top>SKY_HUD_TOP?top:SKY_HUD_TOP;
    int r=right<SKY_HUD_RIGHT?right:SKY_HUD_RIGHT,b=bottom<SKY_HUD_BOTTOM?bottom:SKY_HUD_BOTTOM;
    if(l<r)for(int y=t;y<b;++y)
        memcpy(screen+y*stride+l,hud.Panel()+(y-SKY_HUD_TOP)*SKY_HUD_W+l-SKY_HUD_LEFT,(r-l)*sizeof(Pixel));
    Pixel foreground=HudColor(220,239,250);
    Text(screen,stride,4,SKY_DISPLAY_H-12,"MENU",left,top,right,bottom,foreground);
    Text(screen,stride,SKY_DISPLAY_W-30,SKY_DISPLAY_H-12,"EXIT",left,top,right,bottom,foreground);
}
