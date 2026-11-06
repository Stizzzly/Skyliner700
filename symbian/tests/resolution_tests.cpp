#include "soft3d.h"
#include "hud.h"
#include <stdio.h>
#include <stdlib.h>
#include <vector>
static Soft3D renderer;
static void Check(bool ok,const char* message){if(!ok){fprintf(stderr,"FAIL: %s\n",message);exit(1);}}
int main(){
    FlightModel f;FlightModel_Reset(&f);unsigned hashes[2]={2166136261u,2166136261u};
    const int capacityStride=SKY_W+8;
    std::vector<Pixel> pixels(16+capacityStride*SKY_H+16);
    for(int frame=0;frame<80;++frame){FlightState s=f.state;
        if(frame){s.x=(Fix)((frame%9-4)*130);s.z=(Fix)((frame%13-6)*170);s.y=(Fix)(frame%7)*35;s.yaw=frame*Fix::Raw(34734);s.pitch=Fix::Raw(19661);s.roll=Fix::Raw(39322);}
        for(int mode=0;mode<2;++mode){
            renderer.SetHalfResolution(mode!=0);
            int width=mode?88:176,height=mode?104:208;
            int stride=width+(frame&1?8:0);
            Check(renderer.Width()==width&&renderer.Height()==height,"mode dimensions");
            for(unsigned i=0;i<pixels.size();++i)pixels[i]=0xA55A;
            renderer.Render(&pixels[16],stride,s);
            for(int y=0;y<SKY_H;++y)for(int x=0;x<stride;++x){
                Pixel p=pixels[16+y*stride+x];
                if(x<width&&y<height){hashes[mode]^=p;hashes[mode]*=16777619u;}
                else Check(p==0xA55A,"inactive rows/columns or stride overwritten after switch");
            }
            for(int i=0;i<16;++i)Check(pixels[i]==0xA55A&&pixels[pixels.size()-1-i]==0xA55A,"outer guards after switch");
        }
    }
    Check(hashes[0]==0xaf33ac27u,"native frames differ from 0.1.41");
    Check(hashes[1]==0x86c26771u,"half-resolution frames differ from 0.1.40");
    // Exercise the actual direct-screen target with every halfword parity and
    // padding: its opaque HUD must remain untouched, all other pixels exact.
    for(int mode=0;mode<2;++mode)for(int frame=0;frame<80;++frame){
        renderer.SetHalfResolution(mode!=0);
        int w=renderer.Width(),h=renderer.Height(),stride=w+(frame%3),offset=16+(frame&1);
        std::vector<Pixel> reference(stride*h+40,0xA55A),screen(reference.size(),0xA55A);
        FlightState s=f.state;
        s.x=(Fix)((frame%9-4)*130);s.z=(Fix)((frame%13-6)*170);s.y=(Fix)(frame%7)*35;
        s.yaw=frame*Fix::Raw(34734);s.pitch=Fix::Raw(19661);s.roll=Fix::Raw(39322);
        renderer.Render(&reference[offset],stride,s);
        renderer.Render(&screen[offset],stride,s,true);
        for(unsigned i=0;i<screen.size();++i){
            int index=(int)i-offset,y=index>=0?index/stride:-1,x=index>=0?index%stride:-1;
            bool panel=!mode&&y>=SKY_HUD_TOP&&y<SKY_HUD_BOTTOM&&x>=SKY_HUD_LEFT&&x<SKY_HUD_RIGHT;
            Check(screen[i]==(panel?0xA55A:reference[i]),"direct-screen output/opaque HUD/padding/guards");
        }
        // The RAM pause snapshot after a direct frame is still a complete frame.
        renderer.Render(&screen[offset],stride,s);
        Check(screen==reference,"pause snapshot after direct render");
    }
    puts("PASS: 160 direct-screen/fallback/snapshot cases; HUD untouched, other pixels exact, odd strides and both pointer parities.");
    printf("PASS: 160 resolution renders/switches, preserved guards; native %08x, half %08x match previous releases.\n",hashes[0],hashes[1]);
}
