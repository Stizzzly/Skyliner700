#include "fixed.h"
#include "world/terrain.h"

Fix Terrain_GetHeight(Fix x, Fix z) {
    const Fix airportDistance = FxMax(FxAbs(x) - Fix::Raw(6553600), FxAbs(z) - Fix::Raw(23592960));
    // Flat airport region needs no sine/cosine or normalization.
    if(airportDistance.raw<=0)return Fix::Raw(0);
    const Fix hills = FxSin(x * Fix::Raw(786)) * FxCos(z * Fix::Raw(721)) * Fix::Raw(1114112) + FxSin((x + z) * Fix::Raw(426)) * Fix::Raw(524288);
    if(airportDistance.raw>=5898240)return hills;
    // (raw*65536)/(90*65536) == raw/90. GCC emits constant multiply/shift.
    return hills * Fix::Raw(airportDistance.raw/90);
}

int Terrain_IsRunway(Fix x, Fix z) { return FxAbs(x) <= Fix::Raw(983040) && FxAbs(z) <= Fix::Raw(19660800); }

Fix Terrain_GetSurfaceHeight(Fix x, Fix z) {
    if (Terrain_IsRunway(x, z)) return Fix::Raw(3604);
    if (x >= Fix::Raw(1572864) && x <= Fix::Raw(6225920) && z >= -Fix::Raw(4259840) && z <= Fix::Raw(1966080)) return Fix::Raw(2294);
    return Terrain_GetHeight(x, z);
}
