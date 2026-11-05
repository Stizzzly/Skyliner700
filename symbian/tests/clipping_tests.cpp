// Differential test against the pre-0.1.31 five-plane clipping algorithm.
#define private public
#include "../src/soft3d.cpp"
#undef private
#include <stdio.h>
#include <stdlib.h>
static Soft3D renderer;
static Pixel pixels[SKY_W*SKY_H],expected[SKY_W*SKY_H],expectedDepth[SKY_W*SKY_H];
static unsigned ReferenceOutcode(const Vec& v){
    unsigned code=0;
    for(int plane=0;plane<5;++plane)if(Distance(v,plane)<0)code|=1u<<plane;
    return code;
}
static void CheckOutcode(const Vec& v){
    if(Outcode(v)!=ReferenceOutcode(v)){fprintf(stderr,"Outcode mismatch %d %d %d\n",v.x,v.y,v.z);exit(1);}
}
static void Reference(const Vec& a,const Vec& b,const Vec& c){
    unsigned ca=ReferenceOutcode(a),cb=ReferenceOutcode(b),cc=ReferenceOutcode(c);
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
    const int values[]={(-2147483647-1),-2147483647,-131073,-131072,-1,0,1,131071,131072,131073,2147483646,2147483647};
    for(unsigned a=0;a<sizeof(values)/sizeof(values[0]);++a)
     for(unsigned b=0;b<sizeof(values)/sizeof(values[0]);++b)
      for(unsigned c=0;c<sizeof(values)/sizeof(values[0]);++c){Vec v={values[a],values[b],values[c]};CheckOutcode(v);}
    for(int sample=0;sample<1000000;++sample){
        Vec v;
        seed=seed*1664525u+1013904223u;v.x=(int)seed;
        seed=seed*1664525u+1013904223u;v.y=(int)seed;
        seed=seed*1664525u+1013904223u;v.z=(int)seed;CheckOutcode(v);
        if(sample<10000){
            int xLim=RawMul(v.z,46137),yLim=RawMul(v.z,54526);
            for(int d=-1;d<=1;++d){v.x=xLim+d;v.y=-yLim+d;CheckOutcode(v);v.x=-xLim+d;v.y=yLim+d;CheckOutcode(v);}
        }
    }
    puts("PASS: 1000000 full-range outcodes, signed limits and exact side/near-plane boundaries match five-plane reference.");
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
