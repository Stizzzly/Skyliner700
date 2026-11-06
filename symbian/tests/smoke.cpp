#include "soft3d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static Pixel pixels[(SKY_W+8)*SKY_H+32];
static Soft3D renderer;
static void Check(int ok,const char* msg){if(!ok){fprintf(stderr,"FAIL: %s\n",msg);exit(1);}}
static void Save(const char* path){
    FILE* f=fopen(path,"wb");Check(f!=0,"open preview");fprintf(f,"P6\n%d %d\n255\n",renderer.Width(),renderer.Height());
    for(int y=0;y<renderer.Height();++y)for(int x=0;x<renderer.Width();++x){unsigned p=pixels[16+y*(SKY_W+8)+x];unsigned char rgb[3]={(unsigned char)(((p>>11)&31)*255/31),(unsigned char)(((p>>5)&63)*255/63),(unsigned char)((p&31)*255/31)};fwrite(rgb,1,3,f);}
    fclose(f);
}
int main(){
#ifdef SKY_TEST_HALF_RESOLUTION
    renderer.SetHalfResolution(true);
#endif
    const int width=renderer.Width(),height=renderer.Height();
    unsigned frameHash=2166136261u;
    FlightModel f;FlightModel_Reset(&f);
    for(int frame=0;frame<80;++frame){
        for(unsigned i=0;i<sizeof(pixels)/sizeof(pixels[0]);++i)pixels[i]=0xA55A;
        FlightState s=f.state;
        if(frame){s.x=(Fix)((frame%9-4)*130);s.z=(Fix)((frame%13-6)*170);s.y=(Fix)(frame%7)*35;s.yaw=frame*Fix::Raw(34734);s.pitch=Fix::Raw(19661);s.roll=Fix::Raw(39322);}
        renderer.Render(pixels+16,SKY_W+8,s);
        for(int hy=0;hy<height;++hy)for(int hx=0;hx<width;++hx){frameHash^=pixels[16+hy*(SKY_W+8)+hx];frameHash*=16777619u;}
        for(int g=0;g<16;++g){Check(pixels[g]==0xA55A,"leading guard");Check(pixels[16+(SKY_W+8)*SKY_H+g]==0xA55A,"trailing guard");}
        for(int y=0;y<SKY_H;++y)for(int x=0;x<SKY_W+8;++x)if(y>=height||x>=width)Check(pixels[16+y*(SKY_W+8)+x]==0xA55A,"inactive pixels/stride guard");
        if(frame==0){Save("runway.ppm");int changed=0;Pixel sky=pixels[16];for(int i=0;i<height;++i)for(int j=0;j<width;++j)if(pixels[16+i*(SKY_W+8)+j]!=sky)++changed;Check(changed>1000,"visible scene");}
        if(frame==23)Save("airborne.ppm");
    }
    FlightInput in={0};in.throttle=1;
    for(int i=0;i<2000;++i){in.pitch=(i>500&&i<540)?Fix::Raw(65536):Fix::Raw(0);FlightModel_Step(&f,&in,Fix::Raw(1311));}
    Check(f.state.speed>20,"physics accelerates");
    Check(f.state.altitude>1,"aircraft takes off");
    printf("PASS: 80 camera poses, clipping/stride guards, visible runway, takeoff. Altitude %d m\n",FxInt(f.state.altitude));
    printf("Frame checksum: %08x\n",frameHash);
    return 0;
}
