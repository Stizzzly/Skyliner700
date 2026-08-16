#ifndef SKYLINER_PLANE_H
#define SKYLINER_PLANE_H

#ifdef _XBOX
#include <xtl.h>
#else
#include <windows.h>
#endif
#include <d3d9.h>

#ifdef __cplusplus
extern "C" {
#endif

void *GetPlaneVertices(void);
int GetPlaneVertexCount(void);
int GetPlaneVertexStride(void);
DWORD GetPlaneFVF(void);

#ifdef __cplusplus
}
#endif

#endif
