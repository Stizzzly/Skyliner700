#include <d3d9.h>
#include "renderer.h"

typedef struct {
    float x, y, z;
    DWORD color;
} GroundVertex;

static IDirect3DVertexBuffer9* g_testGroundVB = NULL;
static int g_testGroundVertexCount = 0;

extern IDirect3DDevice9* GetD3D9Device(void);

static void AddQuad(GroundVertex* vertices, int* index, float x0, float z0, float x1, float z1, float y, DWORD color) {
    vertices[(*index)++] = (GroundVertex){x0, y, z0, color};
    vertices[(*index)++] = (GroundVertex){x0, y, z1, color};
    vertices[(*index)++] = (GroundVertex){x1, y, z1, color};
    vertices[(*index)++] = (GroundVertex){x0, y, z0, color};
    vertices[(*index)++] = (GroundVertex){x1, y, z1, color};
    vertices[(*index)++] = (GroundVertex){x1, y, z0, color};
}

int D3D9_TestGroundInit(void) {
    IDirect3DDevice9* device = GetD3D9Device();
    GroundVertex* vertices;
    HRESULT hr;
    int index = 0;
    const int stripeCount = 10;

    if (!device) return 0;
    g_testGroundVertexCount = 12 + stripeCount * 6;
    hr = device->lpVtbl->CreateVertexBuffer(device, sizeof(GroundVertex) * g_testGroundVertexCount, 0,
                                             D3DFVF_XYZ | D3DFVF_DIFFUSE, D3DPOOL_MANAGED,
                                             &g_testGroundVB, NULL);
    if (FAILED(hr)) return 0;
    hr = g_testGroundVB->lpVtbl->Lock(g_testGroundVB, 0, 0, (void**)&vertices, 0);
    if (FAILED(hr)) return 0;

    AddQuad(vertices, &index, -600.0f, -600.0f, 600.0f, 600.0f, 0.0f, D3DCOLOR_XRGB(65, 125, 65));
    AddQuad(vertices, &index, -15.0f, -300.0f, 15.0f, 300.0f, 0.015f, D3DCOLOR_XRGB(58, 61, 65));
    for (int stripe = 0; stripe < stripeCount; ++stripe) {
        const float z = -255.0f + stripe * 55.0f;
        AddQuad(vertices, &index, -1.6f, z, 1.6f, z + 25.0f, 0.03f, D3DCOLOR_XRGB(235, 235, 220));
    }
    g_testGroundVB->lpVtbl->Unlock(g_testGroundVB);
    return 1;
}

void D3D9_TestGroundShutdown(void) {
    if (g_testGroundVB) { g_testGroundVB->lpVtbl->Release(g_testGroundVB); g_testGroundVB = NULL; }
}

void D3D9_RenderTestGround(void) {
    IDirect3DDevice9* device = GetD3D9Device();
    if (!device || !g_testGroundVB) return;
    Renderer_SetWorldMatrix(0.0f, 0.0f, 0.0f, 0.0f);
    device->lpVtbl->SetStreamSource(device, 0, g_testGroundVB, 0, sizeof(GroundVertex));
    device->lpVtbl->SetFVF(device, D3DFVF_XYZ | D3DFVF_DIFFUSE);
    device->lpVtbl->SetTexture(device, 0, NULL);
    device->lpVtbl->SetTextureStageState(device, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    device->lpVtbl->SetTextureStageState(device, 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    device->lpVtbl->SetRenderState(device, D3DRS_ZENABLE, TRUE);
    device->lpVtbl->SetRenderState(device, D3DRS_ZWRITEENABLE, TRUE);
    device->lpVtbl->DrawPrimitive(device, D3DPT_TRIANGLELIST, 0, g_testGroundVertexCount / 3);
}
