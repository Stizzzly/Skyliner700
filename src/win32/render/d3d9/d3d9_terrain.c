#include <d3d9.h>
#include <math.h>
#include <string.h>
#include "renderer.h"
#include "world/terrain.h"

#define TERRAIN_CELLS 64
#define TERRAIN_STEP (1200.0f / TERRAIN_CELLS)
#define TERRAIN_VERTICES (TERRAIN_CELLS * TERRAIN_CELLS * 6)

typedef struct { float x, y, z; DWORD color; float u, v; } TerrainVertex;
typedef struct { float x, y, z; DWORD color; } ColorVertex;

static IDirect3DVertexBuffer9* g_terrainVB;
static IDirect3DVertexBuffer9* g_airportVB;
static IDirect3DVertexBuffer9* g_markingVB;
static IDirect3DVertexBuffer9* g_hangarVB;
static int g_airportVertexCount;
static int g_markingVertexCount;
static int g_hangarVertexCount;

extern IDirect3DDevice9* GetD3D9Device(void);
extern IDirect3DBaseTexture9* GetTerrainGrassTexture(void);
extern IDirect3DBaseTexture9* GetRunwayTexture(void);

static TerrainVertex MakeTerrainVertex(float x, float z) {
    const float height = Terrain_GetHeight(x, z);
    const float tint = fmaxf(0.0f, fminf(1.0f, (height + 26.0f) / 52.0f));
    TerrainVertex vertex;
    vertex.x = x; vertex.y = height; vertex.z = z;
    vertex.color = D3DCOLOR_XRGB((int)(172.0f + tint * 45.0f),
                                 (int)(172.0f + tint * 55.0f),
                                 (int)(165.0f + tint * 38.0f));
    vertex.u = (x + 600.0f) / 35.0f;
    vertex.v = (z + 600.0f) / 35.0f;
    return vertex;
}

static void AddTexturedQuad(TerrainVertex* vertices, int* index,
                            float x0, float z0, float x1, float z1, float y,
                            float u0, float v0, float u1, float v1, DWORD color) {
    vertices[(*index)++] = (TerrainVertex){x0,y,z0,color,u0,v0};
    vertices[(*index)++] = (TerrainVertex){x0,y,z1,color,u0,v1};
    vertices[(*index)++] = (TerrainVertex){x1,y,z1,color,u1,v1};
    vertices[(*index)++] = (TerrainVertex){x0,y,z0,color,u0,v0};
    vertices[(*index)++] = (TerrainVertex){x1,y,z1,color,u1,v1};
    vertices[(*index)++] = (TerrainVertex){x1,y,z0,color,u1,v0};
}

static void AddColorQuad(ColorVertex* vertices, int* index,
                         float x0, float z0, float x1, float z1, float y, DWORD color) {
    vertices[(*index)++] = (ColorVertex){x0,y,z0,color};
    vertices[(*index)++] = (ColorVertex){x0,y,z1,color};
    vertices[(*index)++] = (ColorVertex){x1,y,z1,color};
    vertices[(*index)++] = (ColorVertex){x0,y,z0,color};
    vertices[(*index)++] = (ColorVertex){x1,y,z1,color};
    vertices[(*index)++] = (ColorVertex){x1,y,z0,color};
}

static void AddBox(ColorVertex* vertices, int* index, float x, float z,
                   float halfWidth, float halfDepth, float height) {
    const DWORD wall = D3DCOLOR_XRGB(178, 181, 176);
    const DWORD roof = D3DCOLOR_XRGB(62, 74, 82);
    const float y = 0.10f;
    const float x0 = x - halfWidth, x1 = x + halfWidth;
    const float z0 = z - halfDepth, z1 = z + halfDepth;
    AddColorQuad(vertices, index, x0,z0,x1,z1,y + height,roof);
    /* Four vertical faces, each as two triangles. */
    const ColorVertex faces[] = {
        {x0,y,z0,wall},{x0,y+height,z0,wall},{x1,y+height,z0,wall},{x0,y,z0,wall},{x1,y+height,z0,wall},{x1,y,z0,wall},
        {x1,y,z0,wall},{x1,y+height,z0,wall},{x1,y+height,z1,wall},{x1,y,z0,wall},{x1,y+height,z1,wall},{x1,y,z1,wall},
        {x1,y,z1,wall},{x1,y+height,z1,wall},{x0,y+height,z1,wall},{x1,y,z1,wall},{x0,y+height,z1,wall},{x0,y,z1,wall},
        {x0,y,z1,wall},{x0,y+height,z1,wall},{x0,y+height,z0,wall},{x0,y,z1,wall},{x0,y+height,z0,wall},{x0,y,z0,wall}
    };
    memcpy(&vertices[*index], faces, sizeof(faces));
    *index += (int)(sizeof(faces) / sizeof(faces[0]));
}

int D3D9_TerrainInit(void) {
    IDirect3DDevice9* device = GetD3D9Device();
    TerrainVertex* terrain;
    TerrainVertex* airport;
    ColorVertex* markings;
    ColorVertex* hangars;
    HRESULT hr;
    int index = 0;
    if (!device) return 0;

    hr = device->lpVtbl->CreateVertexBuffer(device, sizeof(TerrainVertex) * TERRAIN_VERTICES, 0,
        D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1, D3DPOOL_MANAGED, &g_terrainVB, NULL);
    if (FAILED(hr)) return 0;
    if (FAILED(g_terrainVB->lpVtbl->Lock(g_terrainVB, 0, 0, (void**)&terrain, 0))) return 0;
    for (int row = 0; row < TERRAIN_CELLS; ++row) {
        const float z0 = -600.0f + row * TERRAIN_STEP;
        const float z1 = z0 + TERRAIN_STEP;
        for (int column = 0; column < TERRAIN_CELLS; ++column) {
            const float x0 = -600.0f + column * TERRAIN_STEP;
            const float x1 = x0 + TERRAIN_STEP;
            terrain[index++] = MakeTerrainVertex(x0,z0); terrain[index++] = MakeTerrainVertex(x0,z1); terrain[index++] = MakeTerrainVertex(x1,z1);
            terrain[index++] = MakeTerrainVertex(x0,z0); terrain[index++] = MakeTerrainVertex(x1,z1); terrain[index++] = MakeTerrainVertex(x1,z0);
        }
    }
    g_terrainVB->lpVtbl->Unlock(g_terrainVB);

    g_airportVertexCount = 12;
    hr = device->lpVtbl->CreateVertexBuffer(device, sizeof(TerrainVertex) * g_airportVertexCount, 0,
        D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1, D3DPOOL_MANAGED, &g_airportVB, NULL);
    if (FAILED(hr) || FAILED(g_airportVB->lpVtbl->Lock(g_airportVB,0,0,(void**)&airport,0))) return 0;
    index = 0;
    AddTexturedQuad(airport,&index,-15,-300,15,300,0.035f,0,0,1.0f,24.0f,D3DCOLOR_XRGB(220,220,220));
    AddTexturedQuad(airport,&index,24,-65,95,30,0.035f,0,0,2.5f,3.0f,D3DCOLOR_XRGB(200,200,200));
    g_airportVB->lpVtbl->Unlock(g_airportVB);

    g_markingVertexCount = 11 * 6;
    hr = device->lpVtbl->CreateVertexBuffer(device, sizeof(ColorVertex) * g_markingVertexCount, 0,
        D3DFVF_XYZ | D3DFVF_DIFFUSE, D3DPOOL_MANAGED, &g_markingVB, NULL);
    if (FAILED(hr) || FAILED(g_markingVB->lpVtbl->Lock(g_markingVB,0,0,(void**)&markings,0))) return 0;
    index = 0;
    for (int stripe = 0; stripe < 11; ++stripe) {
        const float z = -270.0f + stripe * 52.0f;
        AddColorQuad(markings,&index,-1.4f,z,1.4f,z+22.0f,0.055f,D3DCOLOR_XRGB(245,243,218));
    }
    g_markingVB->lpVtbl->Unlock(g_markingVB);

    g_hangarVertexCount = 3 * 30;
    hr = device->lpVtbl->CreateVertexBuffer(device, sizeof(ColorVertex) * g_hangarVertexCount, 0,
        D3DFVF_XYZ | D3DFVF_DIFFUSE, D3DPOOL_MANAGED, &g_hangarVB, NULL);
    if (FAILED(hr) || FAILED(g_hangarVB->lpVtbl->Lock(g_hangarVB,0,0,(void**)&hangars,0))) return 0;
    index = 0;
    AddBox(hangars,&index,58.0f,-30.0f,13.0f,18.0f,10.0f);
    AddBox(hangars,&index,58.0f,18.0f,13.0f,16.0f,9.0f);
    AddBox(hangars,&index,88.0f,-8.0f,9.0f,12.0f,7.0f);
    g_hangarVB->lpVtbl->Unlock(g_hangarVB);
    return 1;
}

void D3D9_TerrainShutdown(void) {
    if (g_hangarVB) { g_hangarVB->lpVtbl->Release(g_hangarVB); g_hangarVB = NULL; }
    if (g_markingVB) { g_markingVB->lpVtbl->Release(g_markingVB); g_markingVB = NULL; }
    if (g_airportVB) { g_airportVB->lpVtbl->Release(g_airportVB); g_airportVB = NULL; }
    if (g_terrainVB) { g_terrainVB->lpVtbl->Release(g_terrainVB); g_terrainVB = NULL; }
}

static DWORD FloatBits(float value) { union { float value; DWORD bits; } converter = { value }; return converter.bits; }

void D3D9_RenderTerrain(void) {
    IDirect3DDevice9* device = GetD3D9Device();
    if (!device || !g_terrainVB) return;
    Renderer_SetWorldMatrix(0,0,0,0);
    device->lpVtbl->SetRenderState(device,D3DRS_FOGENABLE,TRUE);
    device->lpVtbl->SetRenderState(device,D3DRS_FOGCOLOR,D3DCOLOR_XRGB(135,195,218));
    device->lpVtbl->SetRenderState(device,D3DRS_FOGVERTEXMODE,D3DFOG_LINEAR);
    device->lpVtbl->SetRenderState(device,D3DRS_FOGSTART,FloatBits(350.0f));
    device->lpVtbl->SetRenderState(device,D3DRS_FOGEND,FloatBits(1050.0f));
    device->lpVtbl->SetSamplerState(device,0,D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP);
    device->lpVtbl->SetSamplerState(device,0,D3DSAMP_ADDRESSV,D3DTADDRESS_WRAP);
    device->lpVtbl->SetStreamSource(device,0,g_terrainVB,0,sizeof(TerrainVertex));
    device->lpVtbl->SetFVF(device,D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1);
    device->lpVtbl->SetTexture(device,0,GetTerrainGrassTexture());
    device->lpVtbl->SetTextureStageState(device,0,D3DTSS_COLOROP,D3DTOP_MODULATE);
    device->lpVtbl->SetTextureStageState(device,0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
    device->lpVtbl->SetTextureStageState(device,0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
    device->lpVtbl->DrawPrimitive(device,D3DPT_TRIANGLELIST,0,TERRAIN_VERTICES/3);
    device->lpVtbl->SetStreamSource(device,0,g_airportVB,0,sizeof(TerrainVertex));
    device->lpVtbl->SetTexture(device,0,GetRunwayTexture());
    device->lpVtbl->DrawPrimitive(device,D3DPT_TRIANGLELIST,0,g_airportVertexCount/3);
    device->lpVtbl->SetTexture(device,0,NULL);
    device->lpVtbl->SetStreamSource(device,0,g_markingVB,0,sizeof(ColorVertex));
    device->lpVtbl->SetFVF(device,D3DFVF_XYZ|D3DFVF_DIFFUSE);
    device->lpVtbl->SetTextureStageState(device,0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
    device->lpVtbl->SetTextureStageState(device,0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
    device->lpVtbl->DrawPrimitive(device,D3DPT_TRIANGLELIST,0,g_markingVertexCount/3);
    device->lpVtbl->SetStreamSource(device,0,g_hangarVB,0,sizeof(ColorVertex));
    device->lpVtbl->DrawPrimitive(device,D3DPT_TRIANGLELIST,0,g_hangarVertexCount/3);
}
