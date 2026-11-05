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
    Pixel depth[SKY_W*SKY_H];
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
    void ClipIntersectingTriangle(const Vec& a,const Vec& b,const Vec& c,Pixel color);
    void Raster(const Vec& a,const Vec& b,const Vec& c,Pixel color);
    void RasterScreen(const Screen& a,const Screen& b,const Screen& c,Pixel color);
};
#endif
