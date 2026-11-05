// Include the implementation to test the actual private projection helper.
// This test TU replaces soft3d.cpp in its desktop target, never in the phone.
#include "../src/soft3d.cpp"
#include <stdio.h>
#include <stdlib.h>
static void CheckProjection(int x,int y,int z){
    if(DepthInverse(z)!=(unsigned)((1ULL<<40)/(unsigned)z)){fprintf(stderr,"Inverse mismatch: %d\n",z);exit(1);}
    Vec v;v.x=x;v.y=y;v.z=z;Screen actual=Project(v);
    int ex=SKY_W*8+(int)(((Wide)x*1000)/z);
    int ey=SKY_H*8-(int)(((Wide)y*1000)/z);
    int ez=(int)((30000LL*65536)/z);
    if(actual.x!=ex||actual.y!=ey||actual.z!=ez){fprintf(stderr,"Projection mismatch: %d %d %d\n",x,y,z);exit(1);}
}
static void CheckEdge(unsigned n,unsigned d){
    UWide inverse=EdgeInverse(d);
    if(inverse!=(1ULL<<32)/d || EdgeFloor(n,d,inverse)!=n/d){
        fprintf(stderr,"Edge reciprocal/floor mismatch: %u / %u\n",n,d);exit(1);
    }
    if(n && EdgeFloor(n-1,d,inverse)+1!=((UWide)n+d-1)/d){
        fprintf(stderr,"Edge ceil mismatch\n");exit(1);
    }
}
static void CheckArea(Wide n,unsigned d){
    UWide inverse=AreaInverse(d);
    if(inverse!=(1ULL<<63)/d || AreaDivide(n,d,inverse)!=n/(Wide)d){
        fprintf(stderr,"Area inverse/quotient mismatch: %lld / %u\n",n,d);exit(1);
    }
}
static void CheckSpeed(int raw,unsigned speed){
    unsigned inverse=SpeedInverse(speed);
    if(inverse!=(1ULL<<48)/speed || FixedSpeedDivide(raw,speed,inverse)!=(Wide)raw*65536/speed){
        fprintf(stderr,"Speed division mismatch: %d / %u\n",raw,speed);exit(1);
    }
}
static void CheckSpanPairs(){
    unsigned seed=0x6600u;
    for(int sample=0;sample<10000;++sample){
        Pixel pixels[128],depths[128],expectedPixels[128],expectedDepths[128];
        for(int i=0;i<128;++i){
            seed=seed*1664525u+1013904223u;pixels[i]=(Pixel)seed;
            seed=seed*1664525u+1013904223u;depths[i]=(Pixel)seed;
        }
        int offset=8+(sample&1),zoffset=8+((sample>>1)&1),count=sample%105;
        int z=(sample%90000-10000)*256,dz=(sample%2001)-1000,bias=(sample%31)-15;
        Pixel color=(Pixel)(seed>>16);
        if(sample<4){z=100<<8;dz=0;bias=0;count=32;
            for(int i=0;i<count;++i)depths[zoffset+i]=(sample>>(i&1))&1?0:100;
        }
        memcpy(expectedPixels,pixels,sizeof(pixels));memcpy(expectedDepths,depths,sizeof(depths));
        int current=z;
        for(int i=0;i<count;++i){int d=(current>>8)+bias;
            if(d>expectedDepths[zoffset+i]){expectedDepths[zoffset+i]=(Pixel)d;expectedPixels[offset+i]=color;}current+=dz;
        }
        DrawSpan(pixels+offset,depths+zoffset,count,z+bias*256,dz,color);
        if(memcmp(pixels,expectedPixels,sizeof(pixels))||memcmp(depths,expectedDepths,sizeof(depths))){
            fprintf(stderr,"Span pair/alignment/tail mismatch: %d\n",sample);exit(1);
        }
    }
    puts("PASS: 10000 span cases, four Z masks, pointer parity, odd tails and full buffer guards.");
}
int main(){
    CheckSpanPairs();
    const int speedInputs[]={0,1,-1,2147483647,(-2147483647-1),5898240,-5898240};
    const unsigned speedDivisors[]={1310720,1310721,3276800,5898240,5898241,2147483647};
    for(unsigned a=0;a<sizeof(speedInputs)/sizeof(speedInputs[0]);++a)
        for(unsigned b=0;b<sizeof(speedDivisors)/sizeof(speedDivisors[0]);++b)CheckSpeed(speedInputs[a],speedDivisors[b]);
    unsigned speedSeed=0x6600u;
    for(int sample=0;sample<1000000;++sample){
        speedSeed=speedSeed*1664525u+1013904223u;int raw=(int)speedSeed;
        speedSeed=speedSeed*1664525u+1013904223u;unsigned speed=1310720u+(speedSeed%2146172928u);
        CheckSpeed(raw,speed);CheckSpeed(raw,1310720u);CheckSpeed(raw,3276800u);
        unsigned large=(speedSeed&0x7fffffffu)+1u;
        unsigned small=((unsigned)raw&0x7fffffffu)%large;
        if(OctantRatio(small,large)!=((UWide)small<<24)/large || OctantRatio(large,large)!=(1u<<24)){
            fprintf(stderr,"Octant ratio mismatch\n");exit(1);
        }
    }
    puts("PASS: 1,000,000 exact speed/constant division and atan octant ratio comparisons.");
    const Wide areaNumerators[]={0,1,-1,2147483647LL,-2147483648LL,
        (1LL<<48)-1,-(1LL<<48),9223372036854775807LL,(-9223372036854775807LL-1)};
    for(unsigned d=1;d<=65536;++d){
        for(unsigned k=0;k<sizeof(areaNumerators)/sizeof(areaNumerators[0]);++k)CheckArea(areaNumerators[k],d);
    }
    for(int bit=0;bit<=30;++bit)for(int bin=256;bin<512;++bin){
        unsigned center=(unsigned)(((UWide)bin<<23)>>(31-bit));
        for(int delta=-1;delta<=1;++delta){
            Wide d=(Wide)center+delta;
            if(d>0&&d<=2147483647LL){CheckArea(9223372036854775807LL,(unsigned)d);CheckArea((-9223372036854775807LL-1),(unsigned)d);}
        }
    }
    unsigned areaSeed=0x92006600u;
    for(int sample=0;sample<1000000;++sample){
        areaSeed=areaSeed*1664525u+1013904223u;unsigned hi=areaSeed;
        areaSeed=areaSeed*1664525u+1013904223u;unsigned lo=areaSeed;
        areaSeed=areaSeed*1664525u+1013904223u;unsigned d=areaSeed&0x7fffffffu;if(!d)d=1;
        CheckArea((Wide)(((UWide)hi<<32)|lo),d);
    }
    puts("PASS: shared Q63 area reciprocal, signed 64-bit limits and 1,000,000 random quotients.");
    const unsigned edgeNumerators[]={0,1,2,15,16,17,65535,65536,2147483647u,2147483648u};
    for(unsigned d=1;d<=65536;++d){
        for(unsigned i=0;i<sizeof(edgeNumerators)/sizeof(edgeNumerators[0]);++i)CheckEdge(edgeNumerators[i],d);
        CheckEdge(d-1,d);CheckEdge(d,d);CheckEdge(d+1,d);
    }
    unsigned edgeSeed=0x6600u;
    for(int sample=0;sample<1000000;++sample){
        edgeSeed=edgeSeed*1664525u+1013904223u;unsigned d=(edgeSeed&0x7fffffffu)+1u;
        if(d==0x80000000u)--d;
        edgeSeed=edgeSeed*1664525u+1013904223u;CheckEdge(edgeSeed&0x7fffffffu,d);
    }
    puts("PASS: exact edge reciprocals, floor/ceil boundaries and 1,000,000 random edge bounds.");
    const int positions[]={0,1,-1,65536,-65536,2147483647,(-2147483647-1)};
    const int depths[]={131072,131073,131074,262143,262144,16777215,2147483647};
    for(unsigned a=0;a<sizeof(positions)/sizeof(positions[0]);++a)
        for(unsigned b=0;b<sizeof(positions)/sizeof(positions[0]);++b)
            for(unsigned c=0;c<sizeof(depths)/sizeof(depths[0]);++c)CheckProjection(positions[a],positions[b],depths[c]);
    // Every normalization exponent and seed boundary, both sides.
    for(int bit=17;bit<=30;++bit)for(int bin=256;bin<512;++bin){
        unsigned center=(unsigned)(((UWide)bin<<23)>>(31-bit));
        for(int delta=-1;delta<=1;++delta){
            int z=(int)(center+delta);
            if(z>=131072)CheckProjection(2147483647,(-2147483647-1),z);
        }
    }
    unsigned seed=0x6600920u;
    for(int sample=0;sample<1000000;++sample){
        seed=seed*1664525u+1013904223u;int x=(int)seed;
        seed=seed*1664525u+1013904223u;int y=(int)seed;
        seed=seed*1664525u+1013904223u;int z=131072+(int)((seed&0x7fffffffu)%2147352576u);
        CheckProjection(x,y,z);
    }
    puts("PASS: exact LUT/Newton division-free projection, boundary cases and 1,000,000 random vectors.");return 0;
}
