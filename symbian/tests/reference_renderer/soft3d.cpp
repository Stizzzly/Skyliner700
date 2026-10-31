#include "soft3d.h"
#include "world/terrain.h"
#include "fixed.h"
#include <string.h>
#include "plane.inc"
void SkyScale2x(const Pixel* source,int sourceStride,Pixel* destination,int destinationStride){
    for(int y=0;y<SKY_H;++y){
        Pixel* first=destination+(y*2)*destinationStride;
        Pixel* second=first+destinationStride;
        for(int x=0;x<SKY_W;++x){Pixel p=source[y*sourceStride+x];first[x*2]=p;first[x*2+1]=p;second[x*2]=p;second[x*2+1]=p;}
    }
}
static Vec V(Fix x,Fix y,Fix z) { Vec v={x,y,z}; return v; }
static Vec Add(Vec a,Vec b) { return V(a.x+b.x,a.y+b.y,a.z+b.z); }
static Vec Sub(Vec a,Vec b) { return V(a.x-b.x,a.y-b.y,a.z-b.z); }
static Vec Mul(Vec a,Fix s) { return V(a.x*s,a.y*s,a.z*s); }
static Fix Dot(Vec a,Vec b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
static Vec Cross(Vec a,Vec b) { return V(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x); }
static Pixel RGB(int r,int g,int b) { return (Pixel)(((r>>3)<<11)|((g>>2)<<5)|(b>>3)); }
Soft3D::Soft3D():terrainX(0x7fffffff),terrainZ(0x7fffffff),target(0),pitch(SKY_W),depthBias(0) {}
Vec Soft3D::View(Vec p) const { p=Sub(p,eye); return V(Dot(p,right),Dot(p,up),Dot(p,forward)); }
/* Near and side planes keep perspective divisions bounded, including when
   terrain crosses the eye. Focal length 125; screen centre (88,104). */
static Fix Distance(Vec p,int plane) {
    switch(plane) {
    case 0:return p.z-Fix::Raw(131072);
    case 1:return p.x+p.z*Fix::Raw(46137);
    case 2:return -p.x+p.z*Fix::Raw(46137);
    case 3:return p.y+p.z*Fix::Raw(54526);
    default:return -p.y+p.z*Fix::Raw(54526);
    }
}
void Soft3D::Triangle(Vec a,Vec b,Vec c,Pixel color) {
    ClipTriangle(View(a),View(b),View(c),color);
}
static unsigned Outcode(Vec v){
    unsigned code=0;
    for(int plane=0;plane<5;++plane)if(Distance(v,plane)<0)code|=1u<<plane;
    return code;
}
void Soft3D::ClipTriangle(Vec a,Vec b,Vec c,Pixel color) {
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
struct Screen { int x,y,z; };
static Screen Project(Vec v) {
    // Screen coordinates in 1/16 pixel, reciprocal depth in integer units.
    // Project directly with widened integers: no low-precision Q16 reciprocal.
    Screen s;s.x=SKY_W*8+(int)(((Wide)v.x.raw*1000)/v.z.raw);
    s.y=SKY_H*8-(int)(((Wide)v.y.raw*1000)/v.z.raw);
    s.z=(int)((30000LL*65536)/v.z.raw);return s;
}
static int Edge(Screen a,Screen b,int x,int y) {return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);}
// Horizontal span: coverage has already been resolved once for the whole row.
// Only the required Z test remains per pixel. Advance pointers and depth once.
#ifndef SKY_REFERENCE_RASTER
static void DrawSpan(Pixel* dst,Pixel* zbuf,int count,int z,int dz,int bias,Pixel color){
#define SPAN_PIXEL do { int d=(z>>8)+bias; if(d>*zbuf){*zbuf=(Pixel)d;*dst=color;} ++dst;++zbuf;z+=dz; } while(0)
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
void Soft3D::Raster(Vec a,Vec b,Vec c,Pixel color) {
    Screen s[3];s[0]=Project(a);s[1]=Project(b);s[2]=Project(c);
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
    for(int vertex=0;vertex<81;++vertex)terrainView[vertex]=View(terrain[vertex]);
    for(int iz=-4;iz<4;++iz)for(int ix=-4;ix<4;++ix){
        int base=(iz+4)*9+ix+4;
        Vec a=terrainView[base],b=terrainView[base+1],c=terrainView[base+10],d=terrainView[base+9];
        Pixel col=((ix+iz)&1)?RGB(71,112,53):RGB(79,123,60);
        ClipTriangle(a,b,c,col);ClipTriangle(a,c,d,col);
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
        for(int j=0;j<3;++j){Vec p=V(Fix::Raw(plane[i+j].x),Fix::Raw(plane[i+j].y),Fix::Raw(plane[i+j].z));
            Fix x=p.x*cr+p.y*sr,y=-p.x*sr+p.y*cr;
            Fix yy=y*cp-p.z*sp,zz=y*sp+p.z*cp;
            v[j]=V(s.x+x*cy-zz*sy,s.y+yy,s.z+x*sy+zz*cy);
        }
        Triangle(v[0],v[1],v[2],i<48?RGB(205,211,218):RGB(211,66,42));
    }
}
