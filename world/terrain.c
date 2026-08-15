#include <math.h>
#include "terrain.h"

static float Clamp(float value, float minimum, float maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static float Max(float left, float right) {
    return left > right ? left : right;
}

float Terrain_GetHeight(float x, float z) {
    const float hills = sinf(x * 0.012f) * cosf(z * 0.011f) * 17.0f + sinf((x + z) * 0.0065f) * 8.0f;
    const float airportDistance = Max(fabsf(x) - 100.0f, fabsf(z) - 360.0f);
    return hills * Clamp(airportDistance / 90.0f, 0.0f, 1.0f);
}

int Terrain_IsRunway(float x, float z) { return fabsf(x) <= 15.0f && fabsf(z) <= 300.0f; }

float Terrain_GetSurfaceHeight(float x, float z) {
    if (Terrain_IsRunway(x, z)) return 0.055f;
    if (x >= 24.0f && x <= 95.0f && z >= -65.0f && z <= 30.0f) return 0.035f;
    return Terrain_GetHeight(x, z);
}
