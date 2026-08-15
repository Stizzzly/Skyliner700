#ifndef SKYLINER_PLANE_H
#define SKYLINER_PLANE_H

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
