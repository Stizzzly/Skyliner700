#ifndef SKYLINER_TERRAIN_H
#define SKYLINER_TERRAIN_H

/* The renderer and flight model share one deterministic terrain equation,
 * so visual ground and landing collision cannot drift apart. */
float Terrain_GetHeight(float x, float z);
float Terrain_GetSurfaceHeight(float x, float z);
int Terrain_IsRunway(float x, float z);

#endif
