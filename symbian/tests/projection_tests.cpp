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
int main(){
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
