// d3d9_sky.c — fixed-function sky dome и слой облаков

#include <d3d9.h>
#include <math.h>
#include <string.h>
#include "d3dx9_compat.h"
#include "renderer.h"

#define SKY_SEGMENTS 24
#define SKY_RINGS 8
#define SKY_RADIUS 75.0f
#define SKY_VERTEX_COUNT (SKY_SEGMENTS * SKY_RINGS * 6)

typedef struct {
    float x, y, z;
    DWORD color;
    float u, v;
} SkyVertex;

static IDirect3DVertexBuffer9* g_skyVertexBuffer = NULL;

extern IDirect3DDevice9* GetD3D9Device(void);
extern IDirect3DBaseTexture9* GetSkyCloudTexture(void);
extern void D3D9_SetSkyWorldMatrix(void);

static DWORD SkyColor(float elevation) {
    const float horizonRed = 135.0f, horizonGreen = 205.0f, horizonBlue = 240.0f;
    const float zenithRed = 24.0f, zenithGreen = 80.0f, zenithBlue = 188.0f;
    const float blend = elevation * elevation;
    return D3DCOLOR_XRGB(
        (int)(horizonRed + (zenithRed - horizonRed) * blend),
        (int)(horizonGreen + (zenithGreen - horizonGreen) * blend),
        (int)(horizonBlue + (zenithBlue - horizonBlue) * blend));
}

static SkyVertex MakeSkyVertex(float longitude, float elevation) {
    const float angle = elevation * (3.14159265358979323846f * 0.5f);
    const float horizontalRadius = cosf(angle) * SKY_RADIUS;
    SkyVertex vertex;
    vertex.x = cosf(longitude) * horizontalRadius;
    vertex.y = sinf(angle) * SKY_RADIUS;
    vertex.z = sinf(longitude) * horizontalRadius;
    vertex.color = SkyColor(elevation);
    // Повторяем облака: в видимой части купола всегда есть несколько
    // скоплений, а не один большой участок прозрачной текстуры.
    vertex.u = longitude / (3.14159265358979323846f * 2.0f) * 3.0f;
    vertex.v = (1.0f - elevation) * 2.0f;
    return vertex;
}

int D3D9_SkyInit(void) {
    IDirect3DDevice9* device = GetD3D9Device();
    SkyVertex* vertices;
    HRESULT hr;
    int vertexIndex = 0;

    if (!device) return 0;

    hr = device->lpVtbl->CreateVertexBuffer(device, sizeof(SkyVertex) * SKY_VERTEX_COUNT,
                                             0, D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1,
                                             D3DPOOL_MANAGED, &g_skyVertexBuffer, NULL);
    if (FAILED(hr)) return 0;

    hr = g_skyVertexBuffer->lpVtbl->Lock(g_skyVertexBuffer, 0, 0, (void**)&vertices, 0);
    if (FAILED(hr)) {
        g_skyVertexBuffer->lpVtbl->Release(g_skyVertexBuffer);
        g_skyVertexBuffer = NULL;
        return 0;
    }

    for (int ring = 0; ring < SKY_RINGS; ++ring) {
        const float elevation0 = (float)ring / (float)SKY_RINGS;
        const float elevation1 = (float)(ring + 1) / (float)SKY_RINGS;
        for (int segment = 0; segment < SKY_SEGMENTS; ++segment) {
            const float longitude0 = (float)segment / (float)SKY_SEGMENTS * (3.14159265358979323846f * 2.0f);
            const float longitude1 = (float)(segment + 1) / (float)SKY_SEGMENTS * (3.14159265358979323846f * 2.0f);
            const SkyVertex lowerLeft = MakeSkyVertex(longitude0, elevation0);
            const SkyVertex lowerRight = MakeSkyVertex(longitude1, elevation0);
            const SkyVertex upperLeft = MakeSkyVertex(longitude0, elevation1);
            const SkyVertex upperRight = MakeSkyVertex(longitude1, elevation1);

            vertices[vertexIndex++] = lowerLeft;
            vertices[vertexIndex++] = upperLeft;
            vertices[vertexIndex++] = lowerRight;
            vertices[vertexIndex++] = lowerRight;
            vertices[vertexIndex++] = upperLeft;
            vertices[vertexIndex++] = upperRight;
        }
    }

    g_skyVertexBuffer->lpVtbl->Unlock(g_skyVertexBuffer);
    return 1;
}

void D3D9_SkyShutdown(void) {
    if (g_skyVertexBuffer) {
        g_skyVertexBuffer->lpVtbl->Release(g_skyVertexBuffer);
        g_skyVertexBuffer = NULL;
    }
}

void D3D9_RenderSky(void) {
    IDirect3DDevice9* device = GetD3D9Device();
    const float cloudScroll = (float)(GetTickCount() % 180000UL) / 180000.0f;
    D3DXMATRIX textureTransform;

    if (!device || !g_skyVertexBuffer) return;

    D3D9_SetSkyWorldMatrix();
    device->lpVtbl->SetStreamSource(device, 0, g_skyVertexBuffer, 0, sizeof(SkyVertex));
    device->lpVtbl->SetFVF(device, D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);
    device->lpVtbl->SetRenderState(device, D3DRS_ZENABLE, FALSE);
    device->lpVtbl->SetRenderState(device, D3DRS_ZWRITEENABLE, FALSE);
    device->lpVtbl->SetRenderState(device, D3DRS_ALPHABLENDENABLE, FALSE);
    device->lpVtbl->SetTexture(device, 0, NULL);
    device->lpVtbl->SetTextureStageState(device, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    device->lpVtbl->SetTextureStageState(device, 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    device->lpVtbl->SetTextureStageState(device, 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    device->lpVtbl->DrawPrimitive(device, D3DPT_TRIANGLELIST, 0, SKY_VERTEX_COUNT / 3);

    D3DXMatrixTranslation(&textureTransform, cloudScroll, 0.0f, 0.0f);
    device->lpVtbl->SetTexture(device, 0, GetSkyCloudTexture());
    device->lpVtbl->SetSamplerState(device, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    device->lpVtbl->SetSamplerState(device, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
    device->lpVtbl->SetTextureStageState(device, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    device->lpVtbl->SetTextureStageState(device, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device->lpVtbl->SetTextureStageState(device, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    device->lpVtbl->SetTextureStageState(device, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    device->lpVtbl->SetTextureStageState(device, 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
    device->lpVtbl->SetTransform(device, D3DTS_TEXTURE0, &textureTransform);
    device->lpVtbl->SetRenderState(device, D3DRS_ALPHABLENDENABLE, TRUE);
    device->lpVtbl->SetRenderState(device, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->lpVtbl->SetRenderState(device, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    device->lpVtbl->DrawPrimitive(device, D3DPT_TRIANGLELIST, 0, SKY_VERTEX_COUNT / 3);

    device->lpVtbl->SetTextureStageState(device, 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    device->lpVtbl->SetRenderState(device, D3DRS_ALPHABLENDENABLE, FALSE);
    device->lpVtbl->SetRenderState(device, D3DRS_ZENABLE, TRUE);
    device->lpVtbl->SetRenderState(device, D3DRS_ZWRITEENABLE, TRUE);
}
