#ifndef SKY_SOFT3D_H
#define SKY_SOFT3D_H
#include "game/flight.h"
enum { SKY_W=88, SKY_H=104, SKY_DISPLAY_W=176, SKY_DISPLAY_H=208 };
typedef unsigned short Pixel;
void SkyScale2x(const Pixel* source,int sourceStride,Pixel* destination,int destinationStride);
// Plain Q16.16 storage: no Fix constructors/zeroing for temporary vectors.
struct Vec { int x,y,z; };
struct Screen { int x,y,z; };
class Soft3D {
public:
    Soft3D();
    void Render(Pixel* pixels, int stride, const FlightState& state);
    void Triangle(const Vec& a,const Vec& b,const Vec& c,Pixel color);
private:
    enum { TileW=44,TileH=52,TilePixels=TileW*TileH,MaxTriangles=2048 };
    struct Prepared {
        int minx,maxx,miny,maxy,row[3],dx[3],dy[3],dzx,dzy,rowz,bias;
        unsigned divisor[3];UWide inverse[3];Pixel color;
    };
#ifdef SKY_UNTILED_RENDER
    Pixel depth[SKY_W*SKY_H];
#else
    Prepared prepared[MaxTriangles];
    unsigned short bins[4][MaxTriangles];
    int triangleCount,binCount[4];
    // Both active buffers start on 32-byte cache lines; no full-frame Z buffer.
    Pixel tileStorage[TilePixels*2+16];
#endif
    Pixel* zbuffer;
    int clipLeft,clipTop,clipRight,clipBottom,zpitch;
    void DrawPrepared(const Prepared& p);
    void FinishTiles(Pixel* pixels,int stride);
    Vec terrain[9*9];
    Vec terrainView[9*9];
    Screen terrainScreen[9*9];
    unsigned char terrainCodes[9*9];
    int terrainX,terrainZ;
    Pixel* target;
    int pitch;
    int depthBias;
    Vec eye,right,up,forward;
    Vec View(const Vec& p) const;
    void ClipTriangle(const Vec& a,const Vec& b,const Vec& c,Pixel color);
    void Raster(const Vec& a,const Vec& b,const Vec& c,Pixel color);
    void RasterScreen(const Screen& a,const Screen& b,const Screen& c,Pixel color);
};
#endif
