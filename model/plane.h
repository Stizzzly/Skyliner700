#ifndef SKYLINER_PLANE_H
#define SKYLINER_PLANE_H

#include <windows.h>

void *GetPlaneVertices(void);
int GetPlaneVertexCount(void);
int GetPlaneVertexStride(void);
DWORD GetPlaneFVF(void);

#endif
