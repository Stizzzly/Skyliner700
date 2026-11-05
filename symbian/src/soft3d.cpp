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
// Only used on a word-aligned source; preserve alias safety on the desktop.
static inline unsigned ReadPair(const Pixel* p){
#if defined(__MARM__) && defined(__GCC32__)
    unsigned value;
    __asm__("ldr %0, [%1]" : "=r"(value) : "r"(p), "m"(p[0]), "m"(p[1]));
    return value;
#else
    unsigned value;memcpy(&value,p,sizeof(value));return value;
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
        const Pixel* row=source+y*sourceStride;
        if(aligned&&((size_t)row&3)==0){
            for(int x=0;x<SKY_W;x+=2){
                unsigned pair=ReadPair(row+x);
                unsigned low=(pair&65535u)|(pair<<16);
                unsigned high=(pair>>16)|(pair&0xffff0000u);
                WritePair(first+x*2,low);WritePair(second+x*2,low);
                WritePair(first+x*2+2,high);WritePair(second+x*2+2,high);
            }
        }else if(aligned){
            for(int x=0;x<SKY_W;++x){unsigned p=ReadPixel(source+y*sourceStride+x);unsigned packed=p|(p<<16);WritePair(first+x*2,packed);WritePair(second+x*2,packed);}
        }else{
            for(int x=0;x<SKY_W;++x){Pixel p=(Pixel)ReadPixel(source+y*sourceStride+x);first[x*2]=p;first[x*2+1]=p;second[x*2]=p;second[x*2+1]=p;}
        }
    }
}
static inline int RawMul(int a,int b){
#if defined(__MARM__) && defined(__GCC32__)
    int result,hi;
    // hi must be the sign-extended upper half of the assembled Q16.16 result.
    __asm__("smull %0, %1, %2, %3\n\t"
            "mov %0, %0, lsr #16\n\t"
            "orr %0, %0, %1, lsl #16\n\t"
            "teq %1, %0, asr #16\n\t"
            "mvnne %0, %1, asr #31\n\t"
            "eorne %0, %0, #0x80000000"
            : "=&r"(result),"=&r"(hi) : "r"(a),"r"(b) : "cc");
    return result;
#else
    return Fix::Narrow(((Wide)a*b)>>16);
#endif
}
// Exact signed saturation with 32-bit arithmetic; no wide carry/borrow chain.
static inline int RawAdd(int a,int b){
#if defined(__MARM__) && defined(__GCC32__)
    int result;
    __asm__("adds %0, %1, %2\n\t"
            "movvs %0, %0, asr #31\n\t"
            "eorvs %0, %0, #0x80000000"
            : "=r"(result) : "r"(a),"r"(b) : "cc");
    return result;
#else
    unsigned result=(unsigned)a+(unsigned)b;
    if((((unsigned)a^result)&((unsigned)b^result)&0x80000000u)!=0)
        return a<0?(-2147483647-1):2147483647;
    return (int)result;
#endif
}
static inline int RawSub(int a,int b){
#if defined(__MARM__) && defined(__GCC32__)
    int result;
    __asm__("subs %0, %1, %2\n\t"
            "movvs %0, %0, asr #31\n\t"
            "eorvs %0, %0, #0x80000000"
            : "=r"(result) : "r"(a),"r"(b) : "cc");
    return result;
#else
    unsigned result=(unsigned)a-(unsigned)b;
    if((((unsigned)a^(unsigned)b)&((unsigned)a^result)&0x80000000u)!=0)
        return a<0?(-2147483647-1):2147483647;
    return (int)result;
#endif
}
static inline Vec V(Fix x,Fix y,Fix z){Vec v;v.x=x.raw;v.y=y.raw;v.z=z.raw;return v;}
static inline Vec Add(const Vec& a,const Vec& b){Vec v;v.x=RawAdd(a.x,b.x);v.y=RawAdd(a.y,b.y);v.z=RawAdd(a.z,b.z);return v;}
static inline Vec Sub(const Vec& a,const Vec& b){Vec v;v.x=RawSub(a.x,b.x);v.y=RawSub(a.y,b.y);v.z=RawSub(a.z,b.z);return v;}
static inline Vec Mul(const Vec& a,Fix scale){Vec v;v.x=RawMul(a.x,scale.raw);v.y=RawMul(a.y,scale.raw);v.z=RawMul(a.z,scale.raw);return v;}
static inline Fix Dot(const Vec& a,const Vec& b){return Fix::Raw(RawAdd(RawAdd(RawMul(a.x,b.x),RawMul(a.y,b.y)),RawMul(a.z,b.z)));}
static inline Vec Cross(const Vec& a,const Vec& b){Vec v;v.x=RawSub(RawMul(a.y,b.z),RawMul(a.z,b.y));v.y=RawSub(RawMul(a.z,b.x),RawMul(a.x,b.z));v.z=RawSub(RawMul(a.x,b.y),RawMul(a.y,b.x));return v;}
static Pixel RGB(int r,int g,int b) { return (Pixel)(((r>>3)<<11)|((g>>2)<<5)|(b>>3)); }
static void ClearSky(Pixel* destination,int stride){
    const Pixel sky=RGB(90,155,205);
    const unsigned packed=(unsigned)sky|((unsigned)sky<<16);
    Pixel* row=destination;
    for(int y=0;y<SKY_H;++y,row+=stride){
        // Check per row so odd strides and halfword-aligned buffers remain valid.
        if(((size_t)row&3)==0){
            for(int x=0;x<SKY_W;x+=2)WritePair(row+x,packed);
        }else{
            for(int x=0;x<SKY_W;++x)row[x]=sky;
        }
    }
}
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
    const int xLim=RawMul(v.z,46137),yLim=RawMul(v.z,54526);
    unsigned code=v.z<131072?1u:0u;
    if(v.x < -xLim)code|=2u;
    if(v.x > xLim)code|=4u;
    if(v.y < -yLim)code|=8u;
    if(v.y > yLim)code|=16u;
    return code;
}
void Soft3D::ClipTriangle(const Vec& a,const Vec& b,const Vec& c,Pixel color) {
    unsigned ca=Outcode(a),cb=Outcode(b),cc=Outcode(c);
    if(ca&cb&cc)return;
    if(!(ca|cb|cc)){Raster(a,b,c,color);return;}
    ClipIntersectingTriangle(a,b,c,color,ca|cb|cc);
}
// Both callers have already classified the triangle against all five planes.
void Soft3D::ClipIntersectingTriangle(const Vec& a,const Vec& b,const Vec& c,Pixel color,unsigned clipMask) {
    Vec p[12],q[12]; int count=3;
    p[0]=a;p[1]=b;p[2]=c;
    for(int plane=0;plane<5;++plane) {
        if(!(clipMask&(1u<<plane)))continue;
        int n=0;
        Vec s=p[0];Fix ds=Distance(s,plane),firstDistance=ds;
        // Keep the original first edge and fan anchor: no cyclic vertex rotation.
        for(int i=0;i<count;++i) {
            int next=i+1;bool closing=next==count;
            Vec e=p[closing?0:next];
            Fix de=closing?firstDistance:Distance(e,plane);
            if(ds>=0) q[n++]=s;
            if((ds<0)!=(de<0)) q[n++]=Add(s,Mul(Sub(e,s),ds/(ds-de)));
            s=e;ds=de;
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
#ifdef SKY_TEST_PROJECT_COUNTS
static unsigned projectCalls=0;
#endif
static Screen Project(const Vec& v){
#ifdef SKY_TEST_PROJECT_COUNTS
    ++projectCalls;
#endif
    UWide inverse=DepthInverse(v.z);
    Screen s;s.x=SKY_W*8+ReciprocalDivide((Wide)v.x*1000,v.z,inverse);
    s.y=SKY_H*8-ReciprocalDivide((Wide)v.y*1000,v.z,inverse);
    s.z=ReciprocalDivide(30000LL*65536,v.z,inverse);return s;
}
// Shared classification/projection for vertices referenced by several triangles.
void Soft3D::DrawCachedTriangle(const Vec* view,const Screen* screen,const unsigned char* codes,int a,int b,int c,Pixel color){
    unsigned ca=codes[a],cb=codes[b],cc=codes[c];
    if(ca&cb&cc)return;
    unsigned mask=ca|cb|cc;
    if(!mask)RasterScreen(screen[a],screen[b],screen[c],color);
    else ClipIntersectingTriangle(view[a],view[b],view[c],color,mask);
}
unsigned Soft3D::Quad(const Vec& a,const Vec& b,const Vec& c,const Vec& d,Pixel color){
    Vec view[4];view[0]=View(a);view[1]=View(b);view[2]=View(c);view[3]=View(d);
    unsigned c0=Outcode(view[0]),c1=Outcode(view[1]),c2=Outcode(view[2]),c3=Outcode(view[3]);
    unsigned reject=c0&c1&c2&c3;
    if(reject)return reject;
    unsigned m0=c0|c1|c2,m1=c0|c2|c3;
    Screen screen[4];
    // Only accepted triangles consume the projected cache; clipped ones project later.
    if(!m0||!m1){
        screen[0]=Project(view[0]);screen[2]=Project(view[2]);
        if(!m0)screen[1]=Project(view[1]);
        if(!m1)screen[3]=Project(view[3]);
    }
    if(!(c0&c1&c2)){
        if(!m0)RasterScreen(screen[0],screen[1],screen[2],color);
        else ClipIntersectingTriangle(view[0],view[1],view[2],color,m0);
    }
    if(!(c0&c2&c3)){
        if(!m1)RasterScreen(screen[0],screen[2],screen[3],color);
        else ClipIntersectingTriangle(view[0],view[2],view[3],color,m1);
    }
    return 0;
}
unsigned Soft3D::RunwayMarkingsOutside(unsigned mask) const{
    // Markings are higher than the asphalt. Confirm the candidate planes at
    // their height; the expanded footprint encloses every marking quad.
    // 16 raw units conservatively cover fixed-point rounding at the boundary.
    for(int z=-300;z<=300;z+=600)for(int x=-15;x<=15;x+=30){
        Vec v=View(V((Fix)x,Fix::Raw(5243),(Fix)z));
        for(int plane=0;plane<5;++plane)
            if((mask&(1u<<plane))&&Distance(v,plane)>=-16)mask&=~(1u<<plane);
        if(!mask)return 0;
    }
    return mask;
}
static int Edge(Screen a,Screen b,int x,int y) {return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);}
// Horizontal span: coverage has already been resolved once for the whole row.
// Only the required Z test remains per pixel. Advance pointers and depth once.
#ifndef SKY_REFERENCE_RASTER
// Called only with word-aligned destination/depth pointers and at least two pixels.
#if defined(__MARM__) && defined(__GCC32__) && !defined(SKY_SCALAR_SPANS)
#include "span_arm.inc"
#endif
static void DrawSpan(Pixel* dst,Pixel* zbuf,int count,int z,int dz,Pixel color){
#define SPAN_PIXEL do { int d=(z>>8); if(d>(int)ReadPixel(zbuf)){*zbuf=(Pixel)d;*dst=color;} ++dst;++zbuf;z+=dz; } while(0)
#ifndef SKY_SCALAR_SPANS
    // A single prefix pixel aligns both pointers when their halfword parity agrees.
    if(count>0&&((size_t)dst&2)&&((size_t)zbuf&2)){SPAN_PIXEL;--count;}
    // ARM consumes the complete aligned span, including an odd final pixel.
    if(count>=2&&(((size_t)dst|(size_t)zbuf)&3)==0){
        unsigned packed=(unsigned)color|((unsigned)color<<16);
#if defined(__MARM__) && defined(__GCC32__)
        SkyDrawPairsArm(dst,zbuf,count,z,dz,packed);
        return;
#else
        // Portable equivalent: both reads precede tests/stores, preserving all masks.
        while(count>=2){
            unsigned old0=ReadPixel(zbuf),old1=ReadPixel(zbuf+1);
            int d0=(z>>8);z+=dz;int d1=(z>>8);z+=dz;
            bool pass0=d0>(int)old0,pass1=d1>(int)old1;
            if(pass0&&pass1){
                WritePair(dst,packed);
                WritePair(zbuf,(unsigned)(Pixel)d0|((unsigned)(Pixel)d1<<16));
            }else{
                if(pass0){zbuf[0]=(Pixel)d0;dst[0]=color;}
                if(pass1){zbuf[1]=(Pixel)d1;dst[1]=color;}
            }
            dst+=2;zbuf+=2;count-=2;
        }
#endif
    }
    // Different pointer alignment: retain the proven halfword path.
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
    if(minx>maxx||miny>maxy)return;
    int row[3],dx[3],dy[3];
    const int sampleX=minx*16+8,sampleY=miny*16+8;
    row[0]=Edge(s[1],s[2],sampleX,sampleY);
    row[1]=Edge(s[2],s[0],sampleX,sampleY);
    row[2]=Edge(s[0],s[1],sampleX,sampleY);
    dx[0]=-(s[2].y-s[1].y)*16;dy[0]=(s[2].x-s[1].x)*16;
    dx[1]=-(s[0].y-s[2].y)*16;dy[1]=(s[0].x-s[2].x)*16;
    dx[2]=-(s[1].y-s[0].y)*16;dy[2]=(s[1].x-s[0].x)*16;
    UWide inverseArea=AreaInverse((unsigned)area);
    int dzx=(int)AreaDivide(((Wide)dx[0]*s[0].z+(Wide)dx[1]*s[1].z+(Wide)dx[2]*s[2].z)*256,(unsigned)area,inverseArea);
    int dzy=(int)AreaDivide(((Wide)dy[0]*s[0].z+(Wide)dy[1]*s[1].z+(Wide)dy[2]*s[2].z)*256,(unsigned)area,inverseArea);
    int rowz=(int)AreaDivide(((Wide)row[0]*s[0].z+(Wide)row[1]*s[1].z+(Wide)row[2]*s[2].z)*256,(unsigned)area,inverseArea);
#ifndef SKY_REFERENCE_RASTER
    // Apply the constant only after the unbiased plane gradients were rounded.
    rowz+=depthBias*256;
    unsigned edgeDivisor[3];UWide edgeInverse[3];
    for(int edge=0;edge<3;++edge){
        int step=dx[edge];edgeDivisor[edge]=(unsigned)(step<0?-step:step);
        edgeInverse[edge]=step?EdgeInverse(edgeDivisor[edge]):0;
    }
    // Keep base-of-row pointers so the final increment is at most one-past-end.
    Pixel* dstRow=target+miny*pitch;
    Pixel* zRow=depth+miny*SKY_W;
#endif
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
            if(step>0&&w<0){int bound=(int)EdgeFloor((unsigned)(-(w+1)),edgeDivisor[edge],edgeInverse[edge])+1;if(bound>first)first=bound;}
            else if(step<0){if(w<0){last=-1;break;}int bound=(int)EdgeFloor((unsigned)w,edgeDivisor[edge],edgeInverse[edge]);if(bound<last)last=bound;}
            else if(step==0&&w<0){last=-1;break;}
        }
        if(first<=last){
            int start=minx+first;
            DrawSpan(dstRow+start,zRow+start,last-first+1,rowz+dzx*first,dzx,color);
        }
        dstRow+=pitch;zRow+=SKY_W;
#endif
        row[0]+=dy[0];row[1]+=dy[1];row[2]+=dy[2];
        rowz+=dzy;
    }
}
void Soft3D::Render(Pixel* pixels,int stride,const FlightState& s) {
    target=pixels;pitch=stride;depthBias=0;
    memset(depth,0,sizeof(depth));
    ClearSky(target,pitch);
    Fix sy=(Fix)FxSin(s.yaw),cy=(Fix)FxCos(s.yaw);
    eye=V(s.x-sy*17,s.y+6,s.z+cy*17);
    // Chase offset is constant (29 horizontal, -5 vertical). Normalize offline.
    forward=V(sy*Fix::Raw(64583),-Fix::Raw(11135),-cy*Fix::Raw(64583));
    right=V(cy,0,sy);up=Cross(right,forward);
    /* 8x8 cells, 100m each: retain the 800m footprint with 128 triangles. */
    // Cancel the Q16.16 scales before division by constant 100.
    // Keep the former truncation-before-floor behavior at negative boundaries.
    int ox=FxFloor(Fix::Raw(s.x.raw/100))*100,oz=FxFloor(Fix::Raw(s.z.raw/100))*100;
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
        unsigned ca=terrainCodes[base],cb=terrainCodes[base+1];
        unsigned cc=terrainCodes[base+10],cd=terrainCodes[base+9];
        Pixel col=((ix+iz)&1)?RGB(71,112,53):RGB(79,123,60);
        // Reject before copying camera vertices; reuse the per-grid outcodes.
        if(ca&cb&cc){}
        else if(!(ca|cb|cc))
            RasterScreen(terrainScreen[base],terrainScreen[base+1],terrainScreen[base+10],col);
        else ClipIntersectingTriangle(terrainView[base],terrainView[base+1],terrainView[base+10],col,ca|cb|cc);
        if(ca&cc&cd){}
        else if(!(ca|cc|cd))
            RasterScreen(terrainScreen[base],terrainScreen[base+10],terrainScreen[base+9],col);
        else ClipIntersectingTriangle(terrainView[base],terrainView[base+10],terrainView[base+9],col,ca|cc|cd);
    }
    // Polygon offset prevents coplanar runway/ground quantization fighting.
    depthBias=2;
    unsigned runwayReject=Quad(V(-15,Fix::Raw(3604),-300),V(15,Fix::Raw(3604),-300),V(15,Fix::Raw(3604),300),V(-15,Fix::Raw(3604),300),RGB(62,66,70));
    if(!runwayReject||!RunwayMarkingsOutside(runwayReject)){
        depthBias=4;
        for(int mark=-280;mark<300;mark+=40){
            Quad(V(-Fix::Raw(32768),Fix::Raw(5243),(Fix)mark),V(Fix::Raw(32768),Fix::Raw(5243),(Fix)mark),V(Fix::Raw(32768),Fix::Raw(5243),(Fix)mark+18),V(-Fix::Raw(32768),Fix::Raw(5243),(Fix)mark+18),RGB(240,235,205));
        }
    }
    depthBias=0;
    Fix sp=(Fix)FxSin(s.pitch),cp=(Fix)FxCos(s.pitch),sr=(Fix)FxSin(s.roll),cr=(Fix)FxCos(s.roll);
    Vec view[PLANE_VERTEX_COUNT];Screen screen[PLANE_VERTEX_COUNT];unsigned char codes[PLANE_VERTEX_COUNT];
    for(int i=0;i<PLANE_VERTEX_COUNT;++i){
        Fix px=Fix::Raw(plane[i].x),py=Fix::Raw(plane[i].y),pz=Fix::Raw(plane[i].z);
        // Preserve the original fixed-point operation order exactly.
        Fix x=px*cr+py*sr,y=-px*sr+py*cr;
        Fix yy=y*cp-pz*sp,zz=y*sp+pz*cp;
        view[i]=View(V(s.x+x*cy-zz*sy,s.y+yy,s.z+x*sy+zz*cy));
        codes[i]=(unsigned char)Outcode(view[i]);
        if(!codes[i])screen[i]=Project(view[i]);
    }
    for(int i=0;i<PLANE_COUNT;i+=3)
        DrawCachedTriangle(view,screen,codes,planeIndices[i],planeIndices[i+1],planeIndices[i+2],i<48?RGB(205,211,218):RGB(211,66,42));
}
