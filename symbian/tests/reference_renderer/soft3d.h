#ifndef SKY_SOFT3D_H
#define SKY_SOFT3D_H
#include "game/flight.h"
enum { SKY_W=176, SKY_H=208, SKY_DISPLAY_W=176, SKY_DISPLAY_H=208 };
typedef unsigned short Pixel;
struct Vec { Fix x,y,z; };
class Soft3D {
public:
    Soft3D();
    void Render(Pixel* pixels, int stride, const FlightState& state);
    void Triangle(Vec a, Vec b, Vec c, Pixel color);
private:
    Pixel depth[SKY_W*SKY_H];
    Vec terrain[9*9];
    Vec terrainView[9*9];
    int terrainX,terrainZ;
    Pixel* target;
    int pitch;
    int depthBias;
    Vec eye,right,up,forward;
    Vec View(Vec p) const;
    void ClipTriangle(Vec a,Vec b,Vec c,Pixel color);
    void Raster(Vec a,Vec b,Vec c,Pixel color);
};
#endif
