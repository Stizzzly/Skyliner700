#include "soft3d.h"
#include "world/terrain.h"
#include "fixed.h"
#include "reciprocal.h"
#include <string.h>
#include <stddef.h>
#include "plane.inc"
static inline unsigned ReadPixel(const Pixel* p){
#if defined(__MARM__) && defined(__GCC32__)
    unsigned value;
    __asm__("ldrh %0, [%1]" : "=r"(value) : "r"(p), "m"(*p));
    return value;
#else
    return *p;
#endif
}
static inline void WritePair(Pixel* p,unsigned value){
#if defined(__MARM__) && defined(__GCC32__)
    __asm__ __volatile__("str %0, [%1]" : : "r"(value),"r"(p) : "memory");
#else
    // memcpy avoids type-aliasing violations in the portable harness.
    memcpy(p,&value,sizeof(value));
#endif
}
void SkyScale2x(const Pixel* source,int sourceStride,Pixel* destination,int destinationStride){
    const bool aligned=(((size_t)destination&3)==0)&&((destinationStride&1)==0);
    for(int y=0;y<SKY_H;++y){
        Pixel* first=destination+(y*2)*destinationStride;
        Pixel* second=first+destinationStride;
        if(aligned){
            for(int x=0;x<SKY_W;++x){unsigned p=ReadPixel(source+y*sourceStride+x);unsigned packed=p|(p<<16);WritePair(first+x*2,packed);WritePair(second+x*2,packed);}
        }else{
            for(int x=0;x<SKY_W;++x){Pixel p=(Pixel)ReadPixel(source+y*sourceStride+x);first[x*2]=p;first[x*2+1]=p;second[x*2]=p;second[x*2+1]=p;}
        }
    }
}
static inline int RawMul(int a,int b){return Fix::Narrow(((Wide)a*b)>>16);}
static inline int RawAdd(int a,int b){return Fix::Narrow((Wide)a+b);}
static inline int RawSub(int a,int b){return Fix::Narrow((Wide)a-b);}
static inline Vec V(Fix x,Fix y,Fix z){Vec v;v.x=x.raw;v.y=y.raw;v.z=z.raw;return v;}
static inline Vec Add(const Vec& a,const Vec& b){Vec v;v.x=RawAdd(a.x,b.x);v.y=RawAdd(a.y,b.y);v.z=RawAdd(a.z,b.z);return v;}
static inline Vec Sub(const Vec& a,const Vec& b){Vec v;v.x=RawSub(a.x,b.x);v.y=RawSub(a.y,b.y);v.z=RawSub(a.z,b.z);return v;}
static inline Vec Mul(const Vec& a,Fix scale){Vec v;v.x=RawMul(a.x,scale.raw);v.y=RawMul(a.y,scale.raw);v.z=RawMul(a.z,scale.raw);return v;}
static inline Fix Dot(const Vec& a,const Vec& b){return Fix::Raw(RawAdd(RawAdd(RawMul(a.x,b.x),RawMul(a.y,b.y)),RawMul(a.z,b.z)));}
static inline Vec Cross(const Vec& a,const Vec& b){Vec v;v.x=RawSub(RawMul(a.y,b.z),RawMul(a.z,b.y));v.y=RawSub(RawMul(a.z,b.x),RawMul(a.x,b.z));v.z=RawSub(RawMul(a.x,b.y),RawMul(a.y,b.x));return v;}
static Pixel RGB(int r,int g,int b) { return (Pixel)(((r>>3)<<11)|((g>>2)<<5)|(b>>3)); }
Soft3D::Soft3D():terrainX(0x7fffffff),terrainZ(0x7fffffff),target(0),pitch(SKY_W),depthBias(0) {}
Vec Soft3D::View(const Vec& p) const { Vec relative=Sub(p,eye); return V(Dot(relative,right),Dot(relative,up),Dot(relative,forward)); }
/* Near and side planes keep perspective divisions bounded, including when
   terrain crosses the eye. Focal length 125; screen centre (88,104). */
static inline Fix Distance(const Vec& p,int plane) {
    switch(plane) {
    case 0:return Fix::Raw(p.z)-Fix::Raw(131072);
    case 1:return Fix::Raw(p.x)+Fix::Raw(p.z)*Fix::Raw(46137);
    case 2:return -Fix::Raw(p.x)+Fix::Raw(p.z)*Fix::Raw(46137);
    case 3:return Fix::Raw(p.y)+Fix::Raw(p.z)*Fix::Raw(54526);
    default:return -Fix::Raw(p.y)+Fix::Raw(p.z)*Fix::Raw(54526);
    }
}
void Soft3D::Triangle(const Vec& a,const Vec& b,const Vec& c,Pixel color) {
    ClipTriangle(View(a),View(b),View(c),color);
}
static inline unsigned Outcode(const Vec& v){
    unsigned code=0;
    for(int plane=0;plane<5;++plane)if(Distance(v,plane)<0)code|=1u<<plane;
    return code;
}
void Soft3D::ClipTriangle(const Vec& a,const Vec& b,const Vec& c,Pixel color) {
    unsigned ca=Outcode(a),cb=Outcode(b),cc=Outcode(c);
    if(ca&cb&cc)return;
    if(!(ca|cb|cc)){Raster(a,b,c,color);return;}
    Vec p[12],q[12]; int count=3;
    p[0]=a;p[1]=b;p[2]=c;
    for(int plane=0;plane<5;++plane) {
        int n=0;
        for(int i=0;i<count;++i) {
            Vec s=p[i],e=p[(i+1)%count];
            Fix ds=Distance(s,plane),de=Distance(e,plane);
            if(ds>=0) q[n++]=s;
            if((ds<0)!=(de<0)) q[n++]=Add(s,Mul(Sub(e,s),ds/(ds-de)));
        }
        count=n; if(count<3)return;
        for(int j=0;j<count;++j)p[j]=q[j];
    }
    for(int k=1;k<count-1;++k)Raster(p[0],p[k],p[k+1],color);
}
// Exact integer division from one shared Q40 reciprocal. For a Q16.16
// coordinate times 1000, reciprocal rounding underestimates by at most 2.
// Two bounded remainder corrections recover the original quotient exactly.
static inline int ReciprocalDivide(Wide numerator,int denominator,UWide inverse){
    bool negative=numerator<0;
    UWide magnitude=(UWide)(negative?-numerator:numerator);
    unsigned q=(unsigned)((magnitude*inverse)>>40);
    UWide remainder=magnitude-(UWide)q*(unsigned)denominator;
    if(remainder>=(unsigned)denominator){++q;remainder-=(unsigned)denominator;}
    if(remainder>=(unsigned)denominator)++q;
    return negative?-(int)q:(int)q;
}
static Screen Project(const Vec& v){
    UWide inverse=DepthInverse(v.z);
    Screen s;s.x=SKY_W*8+ReciprocalDivide((Wide)v.x*1000,v.z,inverse);
    s.y=SKY_H*8-ReciprocalDivide((Wide)v.y*1000,v.z,inverse);
    s.z=ReciprocalDivide(30000LL*65536,v.z,inverse);return s;
}
static int Edge(Screen a,Screen b,int x,int y) {return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);}
// Horizontal span: coverage has already been resolved once for the whole row.
// Only the required Z test remains per pixel. Advance pointers and depth once.
#ifndef SKY_REFERENCE_RASTER
static void DrawSpan(Pixel* dst,Pixel* zbuf,int count,int z,int dz,int bias,Pixel color){
#define SPAN_PIXEL do { int d=(z>>8)+bias; if(d>(int)ReadPixel(zbuf)){*zbuf=(Pixel)d;*dst=color;} ++dst;++zbuf;z+=dz; } while(0)
#ifndef SKY_SCALAR_SPANS
    while(count>=8){
        SPAN_PIXEL;SPAN_PIXEL;SPAN_PIXEL;SPAN_PIXEL;
        SPAN_PIXEL;SPAN_PIXEL;SPAN_PIXEL;SPAN_PIXEL;
        count-=8;
    }
#endif
    while(count>0){SPAN_PIXEL;--count;}
#undef SPAN_PIXEL
}
#endif
void Soft3D::Raster(const Vec& a,const Vec& b,const Vec& c,Pixel color) {
    Screen aScreen=Project(a),bScreen=Project(b),cScreen=Project(c);
    RasterScreen(aScreen,bScreen,cScreen,color);
}
void Soft3D::RasterScreen(const Screen& a,const Screen& b,const Screen& c,Pixel color){
    Screen s[3];s[0]=a;s[1]=b;s[2]=c;
    int area=Edge(s[0],s[1],s[2].x,s[2].y);
    if(area==0)return;
    /* Two-sided geometry is deliberate: original aircraft has thin tail fins
       and land must remain visible from any camera angle. */
    if(area<0){Screen t=s[1];s[1]=s[2];s[2]=t;area=-area;}
    int minx=s[0].x,maxx=minx,miny=s[0].y,maxy=miny;
    for(int i=1;i<3;++i){if(s[i].x<minx)minx=s[i].x;if(s[i].x>maxx)maxx=s[i].x;if(s[i].y<miny)miny=s[i].y;if(s[i].y>maxy)maxy=s[i].y;}
    minx=minx/16;maxx=(maxx+15)/16;miny=miny/16;maxy=(maxy+15)/16;
    if(minx<0)minx=0;
    if(maxx>=SKY_W)maxx=SKY_W-1;
    if(miny<0)miny=0;
    if(maxy>=SKY_H)maxy=SKY_H-1;
    int row[3],dx[3],dy[3];
    for(int e=0;e<3;++e){Screen u=s[(e+1)%3],v=s[(e+2)%3];row[e]=Edge(u,v,minx*16+8,miny*16+8);dx[e]=-(v.y-u.y)*16;dy[e]=(v.x-u.x)*16;}
    int dzx=(int)((((Wide)dx[0]*s[0].z+(Wide)dx[1]*s[1].z+(Wide)dx[2]*s[2].z)*256)/area);
    int dzy=(int)((((Wide)dy[0]*s[0].z+(Wide)dy[1]*s[1].z+(Wide)dy[2]*s[2].z)*256)/area);
    int rowz=(int)((((Wide)row[0]*s[0].z+(Wide)row[1]*s[1].z+(Wide)row[2]*s[2].z)*256)/area);
    for(int y=miny;y<=maxy;++y){
#ifdef SKY_REFERENCE_RASTER
        // Desktop-only exact coverage oracle for the span implementation.
        int w0=row[0],w1=row[1],w2=row[2],z=rowz;
        for(int x=minx;x<=maxx;++x){
            if(w0>=0&&w1>=0&&w2>=0){int d=(z>>8)+depthBias;int index=y*SKY_W+x;
                if(d>depth[index]){depth[index]=(Pixel)d;target[y*pitch+x]=color;}}
            w0+=dx[0];w1+=dx[1];w2+=dx[2];z+=dzx;
        }
#else
        int first=0,last=maxx-minx;
        for(int edge=0;edge<3;++edge){
            int w=row[edge],step=dx[edge];
            if(step>0&&w<0){int bound=(-w+step-1)/step;if(bound>first)first=bound;}
            else if(step<0){if(w<0){last=-1;break;}int bound=w/(-step);if(bound<last)last=bound;}
            else if(step==0&&w<0){last=-1;break;}
        }
        if(first<=last)DrawSpan(target+y*pitch+minx+first,depth+y*SKY_W+minx+first,last-first+1,rowz+dzx*first,dzx,depthBias,color);
#endif
        for(int j=0;j<3;++j)row[j]+=dy[j];
        rowz+=dzy;
    }
}
void Soft3D::Render(Pixel* pixels,int stride,const FlightState& s) {
    target=pixels;pitch=stride;depthBias=0;
    memset(depth,0,sizeof(depth));
    for(int y=0;y<SKY_H;++y)for(int x=0;x<SKY_W;++x)target[y*pitch+x]=RGB(90,155,205);
    Fix sy=(Fix)FxSin(s.yaw),cy=(Fix)FxCos(s.yaw);
    eye=V(s.x-sy*17,s.y+6,s.z+cy*17);
    // Chase offset is constant (29 horizontal, -5 vertical). Normalize offline.
    forward=V(sy*Fix::Raw(64583),-Fix::Raw(11135),-cy*Fix::Raw(64583));
    right=V(cy,0,sy);up=Cross(right,forward);
    /* 8x8 cells, 100m each: retain the 800m footprint with 128 triangles. */
    int ox=(int)FxFloor(s.x/100)*100,oz=(int)FxFloor(s.z/100)*100;
    if(ox!=terrainX||oz!=terrainZ){
        terrainX=ox;terrainZ=oz;
        for(int tz=0;tz<=8;++tz)for(int tx=0;tx<=8;++tx){
            Fix x=(Fix)(ox+(tx-4)*100),z=(Fix)(oz+(tz-4)*100);
            terrain[tz*9+tx]=V(x,Terrain_GetHeight(x,z),z);
        }
    }
    for(int vertex=0;vertex<81;++vertex){
        terrainView[vertex]=View(terrain[vertex]);
        terrainCodes[vertex]=(unsigned char)Outcode(terrainView[vertex]);
        if(terrainCodes[vertex]==0)terrainScreen[vertex]=Project(terrainView[vertex]);
    }
    for(int iz=-4;iz<4;++iz)for(int ix=-4;ix<4;++ix){
        int base=(iz+4)*9+ix+4;
        Vec a=terrainView[base],b=terrainView[base+1],c=terrainView[base+10],d=terrainView[base+9];
        Pixel col=((ix+iz)&1)?RGB(71,112,53):RGB(79,123,60);
        if(!(terrainCodes[base]|terrainCodes[base+1]|terrainCodes[base+10]))
            RasterScreen(terrainScreen[base],terrainScreen[base+1],terrainScreen[base+10],col);
        else ClipTriangle(a,b,c,col);
        if(!(terrainCodes[base]|terrainCodes[base+10]|terrainCodes[base+9]))
            RasterScreen(terrainScreen[base],terrainScreen[base+10],terrainScreen[base+9],col);
        else ClipTriangle(a,c,d,col);
    }
    // Polygon offset prevents coplanar runway/ground quantization fighting.
    depthBias=2;
    Triangle(V(-15,Fix::Raw(3604),-300),V(15,Fix::Raw(3604),-300),V(15,Fix::Raw(3604),300),RGB(62,66,70));
    Triangle(V(-15,Fix::Raw(3604),-300),V(15,Fix::Raw(3604),300),V(-15,Fix::Raw(3604),300),RGB(62,66,70));
    depthBias=4;
    for(int mark=-280;mark<300;mark+=40){
        Triangle(V(-Fix::Raw(32768),Fix::Raw(5243),(Fix)mark),V(Fix::Raw(32768),Fix::Raw(5243),(Fix)mark),V(Fix::Raw(32768),Fix::Raw(5243),(Fix)mark+18),RGB(240,235,205));
        Triangle(V(-Fix::Raw(32768),Fix::Raw(5243),(Fix)mark),V(Fix::Raw(32768),Fix::Raw(5243),(Fix)mark+18),V(-Fix::Raw(32768),Fix::Raw(5243),(Fix)mark+18),RGB(240,235,205));
    }
    depthBias=0;
    Fix sp=(Fix)FxSin(s.pitch),cp=(Fix)FxCos(s.pitch),sr=(Fix)FxSin(s.roll),cr=(Fix)FxCos(s.roll);
    for(int i=0;i<PLANE_COUNT;i+=3){Vec v[3];
        for(int j=0;j<3;++j){Fix px=Fix::Raw(plane[i+j].x),py=Fix::Raw(plane[i+j].y),pz=Fix::Raw(plane[i+j].z);
            Fix x=px*cr+py*sr,y=-px*sr+py*cr;
            Fix yy=y*cp-pz*sp,zz=y*sp+pz*cp;
            v[j]=V(s.x+x*cy-zz*sy,s.y+yy,s.z+x*sy+zz*cy);
        }
        Triangle(v[0],v[1],v[2],i<48?RGB(205,211,218):RGB(211,66,42));
    }
}
