#include "framebuffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <vector>
static void Check(bool ok,const char* s){if(!ok){fprintf(stderr,"FAIL: %s\n",s);exit(1);}}
static unsigned seed=175;
static unsigned Next(){seed=seed*1664525u+1013904223u;return seed;}
int main(){
    for(int trial=0;trial<1200;++trial){
        int pitch=SKY_DISPLAY_W+(trial%10),offset=16+(trial&1);
        int srcPitch=SKY_W+7;
        int srcHeight=SKY_H;
        std::vector<Pixel> source(srcPitch*srcHeight),out(offset+pitch*SKY_DISPLAY_H+16,0xA55A),ref(out);
        for(unsigned i=0;i<source.size();++i)source[i]=(Pixel)Next();
        // Full, odd boundaries, empty/outside rectangles and two separated clips.
        for(int clip=0;clip<2;++clip){
            int l=(int)(Next()%220)-22,t=(int)(Next()%250)-22;
            int r=(int)(Next()%220)-22,b=(int)(Next()%250)-22;
            if(trial<20){l=t=0;r=SKY_DISPLAY_W;b=SKY_DISPLAY_H;}
            SkyPresentRect(&source[0],srcPitch,&out[offset],pitch,l,t,r,b);
            for(int y=0;y<SKY_DISPLAY_H;++y)for(int x=0;x<SKY_DISPLAY_W;++x)
                if(x>=l&&x<r&&y>=t&&y<b)
                    ref[offset+y*pitch+x]=source[y*srcPitch+x];
        }
        Check(out==ref,"direct framebuffer pixel mapping, clip holes, pitch/alignment and outer guards");
    }
    puts("PASS: 1200 direct framebuffer cases, full/clipped/menu paths, odd bounds, disjoint regions, pitch/alignment and guards.");
    return 0;
}
