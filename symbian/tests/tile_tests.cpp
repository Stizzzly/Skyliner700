#define private public
#include "soft3d.h"
#undef private
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static Soft3D renderer;
static Pixel pixels[(SKY_W+8)*SKY_H+32];
static unsigned seed=19;
static unsigned Next(){seed=seed*1664525u+1013904223u;return seed;}
static void Check(bool ok){if(!ok){fprintf(stderr,"Tile framebuffer guard failure\n");exit(1);}}
int main(){
    unsigned hash=2166136261u;
    for(int frame=0;frame<240;++frame){
        for(unsigned i=0;i<sizeof(pixels)/sizeof(pixels[0]);++i)pixels[i]=0xA55A;
#ifdef SKY_UNTILED_RENDER
        renderer.target=pixels+16;renderer.pitch=SKY_W+8;
        renderer.zbuffer=renderer.depth;renderer.zpitch=SKY_W;
        renderer.clipLeft=renderer.clipTop=0;renderer.clipRight=SKY_W;renderer.clipBottom=SKY_H;
        memset(renderer.depth,0,sizeof(renderer.depth));
        for(int y=0;y<SKY_H;++y)for(int x=0;x<SKY_W;++x)pixels[16+y*(SKY_W+8)+x]=(Pixel)(((90>>3)<<11)|((155>>2)<<5)|(205>>3));
#else
        renderer.triangleCount=0;for(int tile=0;tile<4;++tile)renderer.binCount[tile]=0;
#endif
        for(int tri=0;tri<96;++tri){
            Screen v[3];
            for(int j=0;j<3;++j){v[j].x=(int)(Next()%1800)-200;v[j].y=(int)(Next()%2100)-200;v[j].z=30+(int)(Next()%28000);}
            if(tri<8){ // Edges and coplanar ties meeting at x=44 and y=52.
                v[0].x=0;v[0].y=0;v[1].x=88*16;v[1].y=52*16;
                v[2].x=44*16;v[2].y=104*16;
                for(int j=0;j<3;++j)v[j].z=3000;
            }
            renderer.depthBias=(tri%3)*2;
            renderer.RasterScreen(v[0],v[1],v[2],(Pixel)Next());
        }
        renderer.FinishTiles(pixels+16,SKY_W+8);
        for(int y=0;y<SKY_H;++y){
            for(int x=0;x<SKY_W;++x){hash^=pixels[16+y*(SKY_W+8)+x];hash*=16777619u;}
            for(int x=SKY_W;x<SKY_W+8;++x)Check(pixels[16+y*(SKY_W+8)+x]==0xA55A);
        }
        for(int i=0;i<16;++i){Check(pixels[i]==0xA55A);Check(pixels[16+(SKY_W+8)*SKY_H+i]==0xA55A);}
    }
    printf("PASS: 240 randomized triangle batches, depth ties, tile boundaries and stride guards; hash %08x\n",hash);
}
