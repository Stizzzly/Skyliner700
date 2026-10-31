// Include the implementation to test the actual private projection helper.
// This test TU replaces soft3d.cpp in its desktop target, never in the phone.
#include "../src/soft3d.cpp"
#include <stdio.h>
#include <stdlib.h>
static void CheckProjection(int x,int y,int z){
    Vec v;v.x=x;v.y=y;v.z=z;Screen actual=Project(v);
    int ex=SKY_W*8+(int)(((Wide)x*1000)/z);
    int ey=SKY_H*8-(int)(((Wide)y*1000)/z);
    int ez=(int)((30000LL*65536)/z);
    if(actual.x!=ex||actual.y!=ey||actual.z!=ez){fprintf(stderr,"Projection mismatch: %d %d %d\n",x,y,z);exit(1);}
}
int main(){
    const int positions[]={0,1,-1,65536,-65536,2147483647,(-2147483647-1)};
    const int depths[]={131072,131073,131074,262143,262144,16777215,2147483647};
    for(unsigned a=0;a<sizeof(positions)/sizeof(positions[0]);++a)
        for(unsigned b=0;b<sizeof(positions)/sizeof(positions[0]);++b)
            for(unsigned c=0;c<sizeof(depths)/sizeof(depths[0]);++c)CheckProjection(positions[a],positions[b],depths[c]);
    unsigned seed=0x6600920u;
    for(int sample=0;sample<1000000;++sample){
        seed=seed*1664525u+1013904223u;int x=(int)seed;
        seed=seed*1664525u+1013904223u;int y=(int)seed;
        seed=seed*1664525u+1013904223u;int z=131072+(int)((seed&0x7fffffffu)%2147352576u);
        CheckProjection(x,y,z);
    }
    puts("PASS: exact shared-reciprocal projection, boundary cases and 1,000,000 random vectors.");return 0;
}
