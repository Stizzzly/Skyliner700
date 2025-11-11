// d3d9_mesh.c — загрузка и отрисовка геометрии

#include <d3d9.h>
#include <string.h>
#include "../renderer.h"

extern IDirect3DDevice9* GetD3D9Device();

static IDirect3DVertexBuffer9* g_pVB = NULL;
static int g_vertexCount = 0;
static int g_vertexStride = 0;
static DWORD g_fvf = 0;

int D3D9_CreateMesh(void* vertices, int vertexCount, int vertexStride, DWORD fvf) {
    IDirect3DDevice9* device = GetD3D9Device();
    if (!device) return 0;

    g_vertexCount = vertexCount;
    g_vertexStride = vertexStride;
    g_fvf = fvf;

    if (FAILED(device->CreateVertexBuffer(
        vertexCount * vertexStride,
        0,
        fvf,
        D3DPOOL_MANAGED,
        &g_pVB,
        NULL))) {
        return 0;
    }

    void* pVertices;
    if (FAILED(g_pVB->Lock(0, 0, (void**)&pVertices, 0))) return 0;
    memcpy(pVertices, vertices, vertexCount * vertexStride);
    g_pVB->Unlock();

    return 1;
}

void D3D9_RenderMesh() {
    IDirect3DDevice9* device = GetD3D9Device();
    if (!device || !g_pVB) return;

    device->SetStreamSource(0, g_pVB, 0, g_vertexStride);
    device->SetFVF(g_fvf);
    device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, g_vertexCount / 3);
}

// Экспортируем через интерфейс
int Renderer_CreateMesh(void* vertices, int vertexCount, int vertexStride, DWORD fvf) {
    return D3D9_CreateMesh(vertices, vertexCount, vertexStride, fvf);
}

void Renderer_RenderMesh() {
    D3D9_RenderMesh();
}
