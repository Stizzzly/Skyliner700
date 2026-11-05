// Differential test against the pre-0.1.31 five-plane clipping algorithm.
#define private public
#include "../src/soft3d.cpp"
#undef private
#include <stdio.h>
#include <stdlib.h>
static Soft3D renderer;
static Pixel pixels[SKY_W*SKY_H],expected[SKY_W*SKY_H],expectedDepth[SKY_W*SKY_H];
static void Reference(const Vec& a,const Vec& b,const Vec& c){
    unsigned ca=Outcode(a),cb=Outcode(b),cc=Outcode(c);
    if(ca&cb&cc)return;
    if(!(ca|cb|cc)){renderer.Raster(a,b,c,12345);return;}
    Vec p[12],q[12];int count=3;p[0]=a;p[1]=b;p[2]=c;
    for(int plane=0;plane<5;++plane){
        int n=0;
        for(int i=0;i<count;++i){
            Vec s=p[i],e=p[(i+1)%count];Fix ds=Distance(s,plane),de=Distance(e,plane);
            if(ds>=0)q[n++]=s;
            if((ds<0)!=(de<0))q[n++]=Add(s,Mul(Sub(e,s),ds/(ds-de)));
        }
        count=n;if(count<3)return;
        for(int j=0;j<count;++j)p[j]=q[j];
    }
    for(int k=1;k<count-1;++k)renderer.Raster(p[0],p[k],p[k+1],12345);
}
int main(){
    unsigned seed=6600;
    renderer.target=pixels;renderer.pitch=SKY_W;renderer.depthBias=0;
    for(int sample=0;sample<12000;++sample){
        Vec v[3];
        for(int i=0;i<3;++i){
            seed=seed*1664525u+1013904223u;v[i].x=((int)(seed%20000001)-10000000)*4;
            seed=seed*1664525u+1013904223u;v[i].y=((int)(seed%20000001)-10000000)*4;
            seed=seed*1664525u+1013904223u;v[i].z=((int)(seed%20000001)-4000000)*4;
            if(sample<300){v[i].z=131072+(sample%3-1);v[i].x=(i-1)*131072;v[i].y=(i==1?1:-1)*131072;}
        }
        memset(pixels,0,sizeof(pixels));memset(renderer.depth,0,sizeof(renderer.depth));
        Reference(v[0],v[1],v[2]);memcpy(expected,pixels,sizeof(pixels));memcpy(expectedDepth,renderer.depth,sizeof(expectedDepth));
        memset(pixels,0,sizeof(pixels));memset(renderer.depth,0,sizeof(renderer.depth));
        renderer.ClipTriangle(v[0],v[1],v[2],12345);
        if(memcmp(pixels,expected,sizeof(pixels))||memcmp(renderer.depth,expectedDepth,sizeof(expectedDepth))){fprintf(stderr,"Clipping mismatch %d\n",sample);return 1;}
    }
    puts("PASS: 12000 clipping cases match previous color/depth buffers, including near-plane boundaries.");
}
