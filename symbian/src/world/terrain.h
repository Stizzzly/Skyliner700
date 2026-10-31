#include "fixed.h"
#ifndef SKYLINER_TERRAIN_H
#define SKYLINER_TERRAIN_H

/* The renderer and flight model share one deterministic terrain equation,
 * so visual ground and landing collision cannot drift apart. */
Fix Terrain_GetHeight(Fix x, Fix z);
Fix Terrain_GetSurfaceHeight(Fix x, Fix z);
int Terrain_IsRunway(Fix x, Fix z);

#endif
