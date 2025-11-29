// plane.c — данные модели

#include <d3d9.h>
#include "plane.h"

typedef struct {
    float x, y, z;
    DWORD color;
} Vertex;

static Vertex g_PlaneVertices[] = {
    // Фюзеляж
    { -0.5f,  0.0f,  0.0f,  D3DCOLOR_XRGB(200, 200, 200) },
    {  0.5f,  0.0f,  0.0f,  D3DCOLOR_XRGB(200, 200, 200) },
    {  0.0f,  1.0f,  0.0f,  D3DCOLOR_XRGB(200, 200, 200) },
    { -0.5f,  0.0f,  0.0f,  D3DCOLOR_XRGB(200, 200, 200) },
    {  0.0f,  1.0f,  0.0f,  D3DCOLOR_XRGB(200, 200, 200) },
    { -1.0f,  0.5f,  0.0f,  D3DCOLOR_XRGB(200, 200, 200) },

    // Крылья
    { -1.0f,  0.2f,  0.0f,  D3DCOLOR_XRGB(100, 150, 255) },
    { -3.0f,  0.2f,  0.0f,  D3DCOLOR_XRGB(100, 150, 255) },
    { -2.0f,  0.5f,  0.0f,  D3DCOLOR_XRGB(100, 150, 255) },
    {  1.0f,  0.2f,  0.0f,  D3DCOLOR_XRGB(100, 150, 255) },
    {  3.0f,  0.2f,  0.0f,  D3DCOLOR_XRGB(100, 150, 255) },
    {  2.0f,  0.5f,  0.0f,  D3DCOLOR_XRGB(100, 150, 255) },

    // Хвост
    {  0.0f,  0.0f, -1.0f,  D3DCOLOR_XRGB(180, 180, 180) },
    {  0.0f,  0.8f, -1.0f,  D3DCOLOR_XRGB(180, 180, 180) },
    {  0.3f,  0.4f, -1.0f,  D3DCOLOR_XRGB(180, 180, 180) },
    { -0.5f,  0.1f, -1.0f,  D3DCOLOR_XRGB(160, 160, 160) },
    {  0.5f,  0.1f, -1.0f,  D3DCOLOR_XRGB(160, 160, 160) },
    {  0.0f,  0.1f, -2.0f,  D3DCOLOR_XRGB(160, 160, 160) }
};

#define PLANE_VERTEX_COUNT (sizeof(g_PlaneVertices) / sizeof(Vertex))
#define PLANE_VERTEX_STRIDE sizeof(Vertex)
#define PLANE_FVF (D3DFVF_XYZ | D3DFVF_DIFFUSE)

// Геттеры для main.c
void* GetPlaneVertices() { return g_PlaneVertices; }
int GetPlaneVertexCount() { return PLANE_VERTEX_COUNT; }
int GetPlaneVertexStride() { return PLANE_VERTEX_STRIDE; }
DWORD GetPlaneFVF() { return PLANE_FVF; }
