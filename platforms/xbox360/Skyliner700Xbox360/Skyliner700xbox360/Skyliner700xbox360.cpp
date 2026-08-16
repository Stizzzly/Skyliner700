// Xbox 360 platform bootstrap for Skyliner 700.
// This deliberately contains no PC Win32 code and no runtime D3DX compiler.

#include "stdafx.h"
#include <stdio.h>
#include <string.h>
#include "game/flight.h"
#include "game/flight_scenario.h"
#include "model/plane.h"
#include "world/terrain.h"

#define TERRAIN_CELLS 64
#define TERRAIN_SIZE 1200.0f
#define TERRAIN_STEP (TERRAIN_SIZE / TERRAIN_CELLS)
#define TERRAIN_VERTEX_COUNT (TERRAIN_CELLS * TERRAIN_CELLS * 6)
#define AIRPORT_VERTEX_COUNT 12
#define HANGAR_VERTEX_COUNT 90

typedef struct TerrainVertex
{
    float x, y, z;
    float nx, ny, nz;
    DWORD color;
    float u, v;
} TerrainVertex;

typedef struct HudVertex
{
    float x, y, z, w;
    DWORD color;
} HudVertex;

#define HUD_MAX_VERTICES 12288

typedef enum GameScreen
{
    GAME_MAIN_MENU,
    GAME_PLAYING,
    GAME_PAUSED
} GameScreen;

static Direct3D* g_d3d = NULL;
static D3DDevice* g_device = NULL;
static BOOL g_running = TRUE;
static BOOL g_gamepadConnected = FALSE;
static DWORD g_clearColor = D3DCOLOR_XRGB(82, 169, 220);
static UINT g_renderWidth = 0;
static UINT g_renderHeight = 0;
static LARGE_INTEGER g_lastTick;
static float g_secondsPerTick;
static BOOL g_tiledMsaaEnabled = FALSE;
static GameScreen g_screen = GAME_MAIN_MENU;
static int g_menuSelection = 0;
static BOOL g_startWasDown = FALSE;
static BOOL g_confirmWasDown = FALSE;
static BOOL g_menuUpWasDown = FALSE;
static BOOL g_menuDownWasDown = FALSE;
static BOOL g_menuStickUpWasDown = FALSE;
static BOOL g_menuStickDownWasDown = FALSE;
static BOOL g_freeCamera = FALSE;
static BOOL g_backWasDown = FALSE;
static BOOL g_dpadDownWasDown = FALSE;
static BOOL g_testButtonsWereDown = FALSE;
static float g_freeCameraX = 0.0f;
static float g_freeCameraY = 6.0f;
static float g_freeCameraZ = 18.0f;
static float g_freeCameraYaw = 3.14159265f;
static float g_freeCameraPitch = -0.20f;
static FlightInput g_lastFlightInput = {0};
static FlightScenario g_flightScenario;
static D3DSurface* g_renderTarget = NULL;
static D3DSurface* g_tilingRenderTarget = NULL;
static D3DSurface* g_depthStencil = NULL;
static D3DTexture* g_frontBuffer = NULL;
static D3DTexture* g_sceneResolveTexture = NULL;
static D3DTexture* g_planeTexture = NULL;
static D3DTexture* g_terrainGrassTexture = NULL;
static D3DTexture* g_runwayTexture = NULL;
static D3DVertexBuffer* g_planeBuffer = NULL;
static D3DVertexDeclaration* g_planeDeclaration = NULL;
static D3DVertexShader* g_planeVertexShader = NULL;
static D3DPixelShader* g_planePixelShader = NULL;
static D3DVertexBuffer* g_terrainBuffer = NULL;
static D3DVertexBuffer* g_airportBuffer = NULL;
static D3DVertexBuffer* g_hangarBuffer = NULL;
static D3DVertexDeclaration* g_terrainDeclaration = NULL;
static D3DVertexShader* g_terrainVertexShader = NULL;
static D3DPixelShader* g_terrainPixelShader = NULL;
static D3DVertexDeclaration* g_postDeclaration = NULL;
static D3DVertexShader* g_postVertexShader = NULL;
static D3DPixelShader* g_postPixelShader = NULL;
static D3DVertexDeclaration* g_hudDeclaration = NULL;
static D3DVertexShader* g_hudVertexShader = NULL;
static D3DPixelShader* g_hudPixelShader = NULL;

static const char* g_vertexShaderSource =
"float4x4 WVP : register(c0);"
"struct IN { float4 position : POSITION; float2 uv : TEXCOORD0; };"
"struct OUT { float4 position : POSITION; float2 uv : TEXCOORD0; };"
"OUT main(IN input) { OUT output; output.position = mul(WVP, input.position); output.uv = input.uv; return output; }";

static const char* g_pixelShaderSource =
"sampler PlaneSampler : register(s0);"
"struct IN { float2 uv : TEXCOORD0; };"
"float4 main(IN input) : COLOR { return tex2D(PlaneSampler, input.uv); }";

static const char* g_terrainVertexShaderSource =
"float4x4 WVP : register(c0);"
"float4 CameraPosition : register(c4);"
"float4 SunDirection : register(c5);"
"struct IN { float4 position : POSITION; float3 normal : NORMAL; float4 color : COLOR0; float2 uv : TEXCOORD0; };"
"struct OUT { float4 position : POSITION; float4 color : COLOR0; float2 uv : TEXCOORD0; float fog : TEXCOORD1; };"
"OUT main(IN input) {"
" OUT output; float light = 0.48 + 0.52 * saturate(dot(input.normal, SunDirection.xyz));"
" float distanceToCamera = length(CameraPosition.xyz - input.position.xyz);"
" output.position = mul(WVP, input.position); output.color = input.color * light; output.uv = input.uv;"
" output.fog = saturate((distanceToCamera - 360.0) / 760.0); return output; }";

static const char* g_terrainPixelShaderSource =
"float4 FogColor : register(c0);"
"float TextureAmount : register(c1);"
"sampler TerrainSampler : register(s0);"
"struct IN { float4 color : COLOR0; float2 uv : TEXCOORD0; float fog : TEXCOORD1; };"
"float4 main(IN input) : COLOR { float4 surface = lerp(input.color, tex2D(TerrainSampler, input.uv) * input.color, TextureAmount); return lerp(surface, FogColor, input.fog); }";

static const char* g_postVertexShaderSource =
"struct IN { float2 position : POSITION; };"
"struct OUT { float4 position : POSITION; float2 uv : TEXCOORD0; };"
"OUT main(IN input) { OUT output; output.position = float4(input.position, 0.0, 1.0);"
" output.uv = float2(input.position.x * 0.5 + 0.5, 0.5 - input.position.y * 0.5); return output; }";

static const char* g_postPixelShaderSource =
"sampler SceneSampler : register(s0);"
"struct IN { float2 uv : TEXCOORD0; };"
"float4 main(IN input) : COLOR { return tex2D(SceneSampler, input.uv); }";

static const char* g_hudVertexShaderSource =
"struct IN { float4 position : POSITION; float4 color : COLOR0; };"
"struct OUT { float4 position : POSITION; float4 color : COLOR0; };"
"OUT main(IN input) { OUT output; output.position = input.position; output.color = input.color; return output; }";

static const char* g_hudPixelShaderSource =
"struct IN { float4 color : COLOR0; };"
"float4 main(IN input) : COLOR { return input.color; }";

static HRESULT LoadXboxTexture(const char* gamePath, D3DTexture** texture)
{
    HRESULT result = D3DXCreateTextureFromFileA(g_device, gamePath, texture);
    if (FAILED(result))
    {
        if (strcmp(gamePath, "game:\\assets\\plane_livery.dds") == 0)
            OutputDebugStringA("Skyliner700: missing or invalid game:\\assets\\plane_livery.dds\n");
        else if (strcmp(gamePath, "game:\\assets\\terrain_grass.dds") == 0)
            OutputDebugStringA("Skyliner700: missing or invalid game:\\assets\\terrain_grass.dds\n");
        else if (strcmp(gamePath, "game:\\assets\\runway_asphalt.dds") == 0)
            OutputDebugStringA("Skyliner700: missing or invalid game:\\assets\\runway_asphalt.dds\n");
    }
    return result;
}

static HRESULT CreateRenderer()
{
    D3DPRESENT_PARAMETERS presentation;
    D3DSURFACE_PARAMETERS surfaceParameters;
    D3DVIEWPORT9 viewport;
    XVIDEO_MODE videoMode;
    HRESULT result;

    g_d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!g_d3d)
    {
        OutputDebugStringA("Skyliner700: Direct3DCreate9 failed.\n");
        return E_FAIL;
    }

    ZeroMemory(&presentation, sizeof(presentation));
    XGetVideoMode(&videoMode);
    g_renderWidth = min(videoMode.dwDisplayWidth, 1280);
    g_renderHeight = min(videoMode.dwDisplayHeight, 720);
    presentation.BackBufferWidth = g_renderWidth;
    presentation.BackBufferHeight = g_renderHeight;
    presentation.BackBufferFormat = D3DFMT_X8R8G8B8;
    presentation.FrontBufferFormat = D3DFMT_LE_X8R8G8B8;
    presentation.FrontBufferColorSpace = D3DCOLORSPACE_RGB;
    presentation.MultiSampleType = D3DMULTISAMPLE_NONE;
    presentation.BackBufferCount = 0;
    presentation.EnableAutoDepthStencil = FALSE;
    presentation.DisableAutoBackBuffer = TRUE;
    presentation.DisableAutoFrontBuffer = TRUE;
    presentation.AutoDepthStencilFormat = D3DFMT_D24S8;
    presentation.SwapEffect = D3DSWAPEFFECT_DISCARD;
    // Present is synchronized with the television refresh rate.
    presentation.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

    result = g_d3d->CreateDevice(0, D3DDEVTYPE_HAL, NULL,
                                 D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_BUFFER_2_FRAMES,
                                 &presentation, &g_device);
    if (FAILED(result))
    {
        OutputDebugStringA("Skyliner700: CreateDevice failed.\n");
        return E_FAIL;
    }

    ZeroMemory(&viewport, sizeof(viewport));
    viewport.Width = g_renderWidth;
    viewport.Height = g_renderHeight;
    viewport.MinZ = 0.0f;
    viewport.MaxZ = 1.0f;
    g_device->SetViewport(&viewport);

    /* Predicated tiling uses one 1280x256 4x-MSAA EDRAM tile.  BeginTiling
       records the normal scene once and EndTiling replays it for the three
       screen rects, resolving each into g_sceneResolveTexture in UMA memory. */
    ZeroMemory(&surfaceParameters, sizeof(surfaceParameters));
    result = g_device->CreateTexture(g_renderWidth, g_renderHeight, 1, 0,
                                     presentation.FrontBufferFormat, D3DPOOL_DEFAULT,
                                     &g_frontBuffer, NULL);
    if (FAILED(result)) return result;
    result = g_device->CreateTexture(g_renderWidth, g_renderHeight, 1, 0,
                                     D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT,
                                     &g_sceneResolveTexture, NULL);
    if (FAILED(result)) return result;

    if (g_renderWidth == 1280 && g_renderHeight == 720)
    {
        const UINT tileWidth = XGNextMultiple(1280, GPU_EDRAM_TILE_WIDTH_4X);
        const UINT tileHeight = XGNextMultiple(256, GPU_EDRAM_TILE_HEIGHT_4X);
        const UINT textureAlignedWidth = XGNextMultiple(tileWidth, GPU_TEXTURE_TILE_DIMENSION);
        const UINT textureAlignedHeight = XGNextMultiple(tileHeight, GPU_TEXTURE_TILE_DIMENSION);

        surfaceParameters.Base = 0;
        result = g_device->CreateRenderTarget(textureAlignedWidth, textureAlignedHeight,
                                              D3DFMT_X8R8G8B8, D3DMULTISAMPLE_4_SAMPLES,
                                              0, FALSE, &g_tilingRenderTarget, &surfaceParameters);
        if (SUCCEEDED(result))
        {
            surfaceParameters.Base = g_tilingRenderTarget->Size / GPU_EDRAM_TILE_SIZE;
            surfaceParameters.HierarchicalZBase = 0;
            result = g_device->CreateDepthStencilSurface(textureAlignedWidth, textureAlignedHeight,
                                                          D3DFMT_D24S8, D3DMULTISAMPLE_4_SAMPLES,
                                                          0, FALSE, &g_depthStencil, &surfaceParameters);
        }
        if (SUCCEEDED(result))
        {
            g_tiledMsaaEnabled = TRUE;
            g_device->SetScreenExtentQueryMode(D3DSEQM_PRECLIP);
        }
        else
        {
            OutputDebugStringA("Skyliner700: tiled 4x MSAA surfaces unavailable; using standard resolve path.\n");
            if (g_depthStencil) { g_depthStencil->Release(); g_depthStencil = NULL; }
            if (g_tilingRenderTarget) { g_tilingRenderTarget->Release(); g_tilingRenderTarget = NULL; }
        }
    }

    /* This target aliases EDRAM after tiling has resolved.  It receives the
       full resolved scene through a one-rectangle post pass before Swap. */
    ZeroMemory(&surfaceParameters, sizeof(surfaceParameters));
    result = g_device->CreateRenderTarget(g_renderWidth, g_renderHeight, D3DFMT_X8R8G8B8,
                                          D3DMULTISAMPLE_NONE, 0, FALSE,
                                          &g_renderTarget, &surfaceParameters);
    if (FAILED(result)) return result;
    if (!g_tiledMsaaEnabled)
    {
        surfaceParameters.Base = GPU_EDRAM_TILES / 2;
        result = g_device->CreateDepthStencilSurface(g_renderWidth, g_renderHeight, D3DFMT_D24S8,
                                                      D3DMULTISAMPLE_NONE, 0, FALSE,
                                                      &g_depthStencil, &surfaceParameters);
        if (FAILED(result)) return result;
    }

    OutputDebugStringA(g_tiledMsaaEnabled ? "Skyliner700: 720p 4x MSAA predicated tiling initialized.\n"
                                           : "Skyliner700: Xbox explicit EDRAM resolve path initialized.\n");
    return S_OK;
}

static HRESULT CreateAircraft()
{
    D3DVERTEXELEMENT9 elements[3] = {
        { 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 }, D3DDECL_END()
    };
    ID3DXBuffer* vertexCode = NULL;
    ID3DXBuffer* pixelCode = NULL;
    ID3DXBuffer* errors = NULL;
    void* vertices;
    HRESULT result;

    result = D3DXCompileShader(g_vertexShaderSource, (UINT)strlen(g_vertexShaderSource), NULL, NULL,
                               "main", "vs_2_0", 0, &vertexCode, &errors, NULL);
    if (FAILED(result)) goto fail;
    result = g_device->CreateVertexShader((DWORD*)vertexCode->GetBufferPointer(), &g_planeVertexShader);
    if (FAILED(result)) goto fail;
    vertexCode->Release(); vertexCode = NULL;
    result = D3DXCompileShader(g_pixelShaderSource, (UINT)strlen(g_pixelShaderSource), NULL, NULL,
                               "main", "ps_2_0", 0, &pixelCode, &errors, NULL);
    if (FAILED(result)) goto fail;
    result = g_device->CreatePixelShader((DWORD*)pixelCode->GetBufferPointer(), &g_planePixelShader);
    if (FAILED(result)) goto fail;
    result = g_device->CreateVertexDeclaration(elements, &g_planeDeclaration);
    if (FAILED(result)) goto fail;
    result = g_device->CreateVertexBuffer(GetPlaneVertexCount() * GetPlaneVertexStride(), D3DUSAGE_WRITEONLY,
                                          0, D3DPOOL_MANAGED, &g_planeBuffer, NULL);
    if (FAILED(result)) goto fail;
    result = g_planeBuffer->Lock(0, 0, &vertices, 0);
    if (FAILED(result)) goto fail;
    memcpy(vertices, GetPlaneVertices(), GetPlaneVertexCount() * GetPlaneVertexStride());
    g_planeBuffer->Unlock();
    result = LoadXboxTexture("game:\\assets\\plane_livery.dds", &g_planeTexture);
    if (FAILED(result)) goto fail;
    g_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    if (vertexCode) vertexCode->Release();
    if (pixelCode) pixelCode->Release();
    if (errors) errors->Release();
    return S_OK;
fail:
    if (errors) OutputDebugStringA((char*)errors->GetBufferPointer());
    if (vertexCode) vertexCode->Release();
    if (pixelCode) pixelCode->Release();
    if (errors) errors->Release();
    return result;
}

static HRESULT CreatePostProcess()
{
    D3DVERTEXELEMENT9 elements[2] = {
        { 0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 }, D3DDECL_END()
    };
    ID3DXBuffer* vertexCode = NULL;
    ID3DXBuffer* pixelCode = NULL;
    ID3DXBuffer* errors = NULL;
    HRESULT result;

    result = D3DXCompileShader(g_postVertexShaderSource, (UINT)strlen(g_postVertexShaderSource), NULL, NULL,
                               "main", "vs_2_0", 0, &vertexCode, &errors, NULL);
    if (FAILED(result)) goto fail;
    result = g_device->CreateVertexShader((DWORD*)vertexCode->GetBufferPointer(), &g_postVertexShader);
    if (FAILED(result)) goto fail;
    vertexCode->Release(); vertexCode = NULL;
    result = D3DXCompileShader(g_postPixelShaderSource, (UINT)strlen(g_postPixelShaderSource), NULL, NULL,
                               "main", "ps_2_0", 0, &pixelCode, &errors, NULL);
    if (FAILED(result)) goto fail;
    result = g_device->CreatePixelShader((DWORD*)pixelCode->GetBufferPointer(), &g_postPixelShader);
    if (FAILED(result)) goto fail;
    result = g_device->CreateVertexDeclaration(elements, &g_postDeclaration);
    if (FAILED(result)) goto fail;
    if (vertexCode) vertexCode->Release();
    if (pixelCode) pixelCode->Release();
    if (errors) errors->Release();
    return S_OK;
fail:
    if (errors) OutputDebugStringA((char*)errors->GetBufferPointer());
    if (vertexCode) vertexCode->Release();
    if (pixelCode) pixelCode->Release();
    if (errors) errors->Release();
    return result;
}

static HRESULT CreateHud()
{
    D3DVERTEXELEMENT9 elements[3] = {
        { 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 16, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
        D3DDECL_END()
    };
    ID3DXBuffer* vertexCode = NULL;
    ID3DXBuffer* pixelCode = NULL;
    ID3DXBuffer* errors = NULL;
    HRESULT result;

    result = D3DXCompileShader(g_hudVertexShaderSource, (UINT)strlen(g_hudVertexShaderSource), NULL, NULL,
                               "main", "vs_2_0", 0, &vertexCode, &errors, NULL);
    if (FAILED(result)) goto fail;
    result = g_device->CreateVertexShader((DWORD*)vertexCode->GetBufferPointer(), &g_hudVertexShader);
    if (FAILED(result)) goto fail;
    vertexCode->Release(); vertexCode = NULL;
    result = D3DXCompileShader(g_hudPixelShaderSource, (UINT)strlen(g_hudPixelShaderSource), NULL, NULL,
                               "main", "ps_2_0", 0, &pixelCode, &errors, NULL);
    if (FAILED(result)) goto fail;
    result = g_device->CreatePixelShader((DWORD*)pixelCode->GetBufferPointer(), &g_hudPixelShader);
    if (FAILED(result)) goto fail;
    result = g_device->CreateVertexDeclaration(elements, &g_hudDeclaration);
    if (FAILED(result)) goto fail;
    if (vertexCode) vertexCode->Release();
    if (pixelCode) pixelCode->Release();
    if (errors) errors->Release();
    return S_OK;
fail:
    if (errors) OutputDebugStringA((char*)errors->GetBufferPointer());
    if (vertexCode) vertexCode->Release();
    if (pixelCode) pixelCode->Release();
    if (errors) errors->Release();
    return result;
}

static const unsigned char g_hudDigits[10][7] = {
    {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31},
    {30,1,1,14,1,1,30}, {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8}, {14,17,17,14,17,17,14},
    {14,17,17,15,1,1,14}
};

static const unsigned char* HudGlyphFor(char character)
{
    static const unsigned char blank[7] = {0,0,0,0,0,0,0};
    static const unsigned char minus[7] = {0,0,0,31,0,0,0};
    static const unsigned char plus[7] = {0,4,4,31,4,4,0};
    static const unsigned char a[7] = {14,17,17,31,17,17,17};
    static const unsigned char b[7] = {30,17,17,30,17,17,30};
    static const unsigned char c[7] = {14,17,16,16,16,17,14};
    static const unsigned char d[7] = {30,17,17,17,17,17,30};
    static const unsigned char e[7] = {31,16,16,30,16,16,31};
    static const unsigned char f[7] = {31,16,16,30,16,16,16};
    static const unsigned char g[7] = {14,17,16,23,17,17,14};
    static const unsigned char h[7] = {17,17,17,31,17,17,17};
    static const unsigned char i[7] = {31,4,4,4,4,4,31};
    static const unsigned char k[7] = {17,18,20,24,20,18,17};
    static const unsigned char l[7] = {16,16,16,16,16,16,31};
    static const unsigned char m[7] = {17,27,21,21,17,17,17};
    static const unsigned char n[7] = {17,25,21,19,17,17,17};
    static const unsigned char o[7] = {14,17,17,17,17,17,14};
    static const unsigned char p[7] = {30,17,17,30,16,16,16};
    static const unsigned char r[7] = {30,17,17,30,20,18,17};
    static const unsigned char s[7] = {15,16,16,14,1,1,30};
    static const unsigned char t[7] = {31,4,4,4,4,4,4};
    static const unsigned char u[7] = {17,17,17,17,17,17,14};
    static const unsigned char v[7] = {17,17,17,17,17,10,4};
    static const unsigned char w[7] = {17,17,17,21,21,21,10};
    static const unsigned char x[7] = {17,17,10,4,10,17,17};
    static const unsigned char y[7] = {17,17,10,4,4,4,4};
    if (character >= '0' && character <= '9') return g_hudDigits[character - '0'];
    switch (character)
    {
        case '-': return minus; case '+': return plus; case 'A': return a; case 'B': return b;
        case 'C': return c; case 'D': return d; case 'E': return e; case 'F': return f;
        case 'G': return g; case 'H': return h;
        case 'I': return i; case 'K': return k; case 'L': return l; case 'M': return m;
        case 'N': return n; case 'O': return o; case 'P': return p; case 'R': return r;
        case 'S': return s; case 'T': return t; case 'U': return u; case 'V': return v;
        case 'W': return w; case 'X': return x; case 'Y': return y;
        default: return blank;
    }
}

static void AddHudQuad(HudVertex* vertices, int* count, float x0, float y0, float x1, float y1, DWORD color)
{
    HudVertex vertex;
    const float left = x0 * 2.0f / g_renderWidth - 1.0f;
    const float right = x1 * 2.0f / g_renderWidth - 1.0f;
    const float top = 1.0f - y0 * 2.0f / g_renderHeight;
    const float bottom = 1.0f - y1 * 2.0f / g_renderHeight;
    if (*count + 6 > HUD_MAX_VERTICES) return;
    vertex.z = 0.0f; vertex.w = 1.0f; vertex.color = color;
    vertex.x = left; vertex.y = top; vertices[(*count)++] = vertex;
    vertex.x = left; vertex.y = bottom; vertices[(*count)++] = vertex;
    vertex.x = right; vertex.y = bottom; vertices[(*count)++] = vertex;
    vertex.x = left; vertex.y = top; vertices[(*count)++] = vertex;
    vertex.x = right; vertex.y = bottom; vertices[(*count)++] = vertex;
    vertex.x = right; vertex.y = top; vertices[(*count)++] = vertex;
}

static void AddHudText(HudVertex* vertices, int* count, float x, float y, const char* text, DWORD color)
{
    const float pixel = 2.0f;
    int character;
    for (character = 0; text[character] != '\0'; ++character)
    {
        const unsigned char* glyph = HudGlyphFor(text[character]);
        int row;
        for (row = 0; row < 7; ++row)
        {
            int column;
            for (column = 0; column < 5; ++column)
            {
                if (glyph[row] & (1 << (4 - column)))
                    AddHudQuad(vertices, count, x + column * pixel, y + row * pixel,
                               x + (column + 1) * pixel, y + (row + 1) * pixel, color);
            }
        }
        x += pixel * 6.0f;
    }
}

static void RenderHud(const FlightState* flight)
{
    HudVertex vertices[HUD_MAX_VERTICES];
    char speed[24], altitude[24], pitch[24], bank[24], heading[24], throttle[24];
    const DWORD panel = D3DCOLOR_ARGB(155, 4, 12, 20);
    const DWORD text = D3DCOLOR_ARGB(240, 220, 245, 255);
    float headingDegrees = flight->yaw * 57.2957795f;
    int count = 0;
    while (headingDegrees < 0.0f) headingDegrees += 360.0f;
    while (headingDegrees >= 360.0f) headingDegrees -= 360.0f;
    sprintf(speed, "SPD %03d KMH", (int)(flight->speed * 3.6f + 0.5f));
    sprintf(altitude, "ALT %04d M", (int)(flight->altitude + 0.5f));
    sprintf(pitch, "PIT %+03d", (int)(flight->pitch * 57.2957795f + (flight->pitch >= 0.0f ? 0.5f : -0.5f)));
    sprintf(bank, "BNK %+03d", (int)(flight->roll * 57.2957795f + (flight->roll >= 0.0f ? 0.5f : -0.5f)));
    sprintf(heading, "HDG %03d", (int)(headingDegrees + 0.5f));
    sprintf(throttle, "THR %03d", (int)(flight->throttle * 100.0f + 0.5f));
    AddHudQuad(vertices, &count, 10.0f, 10.0f, 178.0f, 106.0f, panel);
    AddHudText(vertices, &count, 16.0f, 15.0f, speed, text);
    AddHudText(vertices, &count, 16.0f, 30.0f, altitude, text);
    AddHudText(vertices, &count, 16.0f, 45.0f, pitch, text);
    AddHudText(vertices, &count, 16.0f, 60.0f, bank, text);
    AddHudText(vertices, &count, 16.0f, 75.0f, heading, text);
    AddHudText(vertices, &count, 16.0f, 90.0f, throttle, text);
    if (g_freeCamera)
    {
        AddHudQuad(vertices, &count, (float)g_renderWidth - 126.0f, 10.0f,
                   (float)g_renderWidth - 10.0f, 35.0f, panel);
        AddHudText(vertices, &count, (float)g_renderWidth - 120.0f, 15.0f, "CAM FREE", text);
    }

    g_device->SetTexture(0, NULL);
    g_device->SetVertexDeclaration(g_hudDeclaration);
    g_device->SetVertexShader(g_hudVertexShader);
    g_device->SetPixelShader(g_hudPixelShader);
    g_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    g_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    g_device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, count / 3, vertices, sizeof(HudVertex));
    g_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
}

static void RenderDevHud(const FlightState* flight)
{
    HudVertex vertices[HUD_MAX_VERTICES];
    char throttle[20], verticalSpeed[20], lift[20], drag[20], ground[20], input[24];
    const char* status = FlightScenario_Status(&g_flightScenario);
    const BOOL showStatus = strcmp(status, "TST OFF") != 0;
    const DWORD panel = D3DCOLOR_ARGB(155, 4, 12, 20);
    const DWORD text = D3DCOLOR_ARGB(240, 255, 228, 170);
    float y = 117.0f;
    int count = 0;

    if (!showStatus && !g_flightScenario.telemetryEnabled) return;
    AddHudQuad(vertices, &count, 10.0f, 112.0f, 226.0f,
               g_flightScenario.telemetryEnabled ? 225.0f : 137.0f, panel);
    if (showStatus)
    {
        AddHudText(vertices, &count, 16.0f, y, status, text);
        y += 15.0f;
    }
    if (g_flightScenario.telemetryEnabled)
    {
        sprintf(throttle, "THR %03d", (int)(flight->throttle * 100.0f + 0.5f));
        sprintf(verticalSpeed, "VSP %+03d", (int)(flight->verticalSpeed + (flight->verticalSpeed >= 0.0f ? 0.5f : -0.5f)));
        sprintf(lift, "LFT %03d", (int)(flight->lift + 0.5f));
        sprintf(drag, "DRG %03d", (int)(flight->drag + 0.5f));
        sprintf(ground, "GND %d", flight->onGround ? 1 : 0);
        sprintf(input, "INP %+.0f%+.0f%+.0f", g_lastFlightInput.pitch, g_lastFlightInput.roll, g_lastFlightInput.yaw);
        AddHudText(vertices, &count, 16.0f, y, throttle, text); y += 15.0f;
        AddHudText(vertices, &count, 16.0f, y, verticalSpeed, text); y += 15.0f;
        AddHudText(vertices, &count, 16.0f, y, lift, text); y += 15.0f;
        AddHudText(vertices, &count, 16.0f, y, drag, text); y += 15.0f;
        AddHudText(vertices, &count, 16.0f, y, ground, text); y += 15.0f;
        AddHudText(vertices, &count, 16.0f, y, input, text);
    }
    g_device->SetTexture(0, NULL);
    g_device->SetVertexDeclaration(g_hudDeclaration);
    g_device->SetVertexShader(g_hudVertexShader);
    g_device->SetPixelShader(g_hudPixelShader);
    g_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    g_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    g_device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, count / 3, vertices, sizeof(HudVertex));
    g_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
}

static float HudTextWidth(const char* text)
{
    return (float)strlen(text) * 12.0f;
}

static void RenderMenu()
{
    HudVertex vertices[HUD_MAX_VERTICES];
    const BOOL paused = g_screen == GAME_PAUSED;
    const char* title = paused ? "PAUSED" : "SKYLINER 700";
    const char* mainItems[2] = { "START GAME", "EXIT GAME" };
    const char* pauseItems[3] = { "RESUME", "RETURN MENU", "EXIT GAME" };
    const int itemCount = paused ? 3 : 2;
    const DWORD dim = D3DCOLOR_ARGB(145, 0, 0, 0);
    const DWORD panel = D3DCOLOR_ARGB(228, 7, 20, 34);
    const DWORD border = D3DCOLOR_ARGB(255, 80, 184, 230);
    const DWORD regular = D3DCOLOR_ARGB(235, 220, 239, 250);
    const DWORD selected = D3DCOLOR_ARGB(255, 255, 228, 170);
    const float panelWidth = 290.0f;
    const float panelHeight = paused ? 164.0f : 132.0f;
    const float panelX = ((float)g_renderWidth - panelWidth) * 0.5f;
    const float panelY = ((float)g_renderHeight - panelHeight) * 0.5f;
    int item;
    int count = 0;

    AddHudQuad(vertices, &count, 0.0f, 0.0f, (float)g_renderWidth, (float)g_renderHeight, dim);
    AddHudQuad(vertices, &count, panelX, panelY, panelX + panelWidth, panelY + panelHeight, panel);
    AddHudQuad(vertices, &count, panelX, panelY, panelX + panelWidth, panelY + 2.0f, border);
    AddHudQuad(vertices, &count, panelX, panelY + panelHeight - 2.0f, panelX + panelWidth, panelY + panelHeight, border);
    AddHudText(vertices, &count, ((float)g_renderWidth - HudTextWidth(title)) * 0.5f, panelY + 18.0f, title, regular);
    for (item = 0; item < itemCount; ++item)
    {
        const char* label = paused ? pauseItems[item] : mainItems[item];
        const DWORD color = item == g_menuSelection ? selected : regular;
        const float y = panelY + 55.0f + item * 30.0f;
        if (item == g_menuSelection)
            AddHudQuad(vertices, &count, panelX + 24.0f, y - 4.0f, panelX + panelWidth - 24.0f, y + 20.0f,
                       D3DCOLOR_ARGB(100, 35, 105, 145));
        AddHudText(vertices, &count, ((float)g_renderWidth - HudTextWidth(label)) * 0.5f, y, label, color);
    }

    g_device->SetTexture(0, NULL);
    g_device->SetVertexDeclaration(g_hudDeclaration);
    g_device->SetVertexShader(g_hudVertexShader);
    g_device->SetPixelShader(g_hudPixelShader);
    g_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    g_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    g_device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, count / 3, vertices, sizeof(HudVertex));
    g_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
}

static float Clamp01(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static TerrainVertex MakeTerrainVertex(float x, float z)
{
    const float height = Terrain_GetHeight(x, z);
    const float leftHeight = Terrain_GetHeight(x - TERRAIN_STEP, z);
    const float rightHeight = Terrain_GetHeight(x + TERRAIN_STEP, z);
    const float nearHeight = Terrain_GetHeight(x, z - TERRAIN_STEP);
    const float farHeight = Terrain_GetHeight(x, z + TERRAIN_STEP);
    const float normalX = leftHeight - rightHeight;
    const float normalY = TERRAIN_STEP * 2.0f;
    const float normalZ = nearHeight - farHeight;
    const float normalLength = sqrtf(normalX * normalX + normalY * normalY + normalZ * normalZ);
    const float tint = Clamp01((height + 26.0f) / 52.0f);
    TerrainVertex vertex;

    vertex.x = x;
    vertex.y = height;
    vertex.z = z;
    vertex.nx = normalX / normalLength;
    vertex.ny = normalY / normalLength;
    vertex.nz = normalZ / normalLength;
    vertex.color = D3DCOLOR_XRGB((int)(72.0f + tint * 28.0f),
                                 (int)(102.0f + tint * 48.0f),
                                 (int)(51.0f + tint * 23.0f));
    vertex.u = (x + TERRAIN_SIZE * 0.5f) / 35.0f;
    vertex.v = (z + TERRAIN_SIZE * 0.5f) / 35.0f;
    return vertex;
}

static TerrainVertex MakeAirportVertex(float x, float z, float u, float v, DWORD color)
{
    TerrainVertex vertex;
    vertex.x = x;
    vertex.y = Terrain_GetSurfaceHeight(x, z) + 0.01f;
    vertex.z = z;
    vertex.nx = 0.0f;
    vertex.ny = 1.0f;
    vertex.nz = 0.0f;
    vertex.color = color;
    vertex.u = u;
    vertex.v = v;
    return vertex;
}

static TerrainVertex MakeHangarVertex(float x, float y, float z,
                                      float nx, float ny, float nz, DWORD color)
{
    TerrainVertex vertex;
    vertex.x = x; vertex.y = y; vertex.z = z;
    vertex.nx = nx; vertex.ny = ny; vertex.nz = nz;
    vertex.color = color;
    vertex.u = 0.0f; vertex.v = 0.0f;
    return vertex;
}

static void AddAirportQuad(TerrainVertex* vertices, int* index,
                           float x0, float z0, float x1, float z1,
                           float u0, float v0, float u1, float v1, DWORD color)
{
    vertices[(*index)++] = MakeAirportVertex(x0, z0, u0, v0, color);
    vertices[(*index)++] = MakeAirportVertex(x0, z1, u0, v1, color);
    vertices[(*index)++] = MakeAirportVertex(x1, z1, u1, v1, color);
    vertices[(*index)++] = MakeAirportVertex(x0, z0, u0, v0, color);
    vertices[(*index)++] = MakeAirportVertex(x1, z1, u1, v1, color);
    vertices[(*index)++] = MakeAirportVertex(x1, z0, u1, v0, color);
}

static void AddHangarQuad(TerrainVertex* vertices, int* index,
                          float x0, float y0, float z0, float x1, float y1, float z1,
                          float nx, float ny, float nz, DWORD color)
{
    vertices[(*index)++] = MakeHangarVertex(x0, y0, z0, nx, ny, nz, color);
    vertices[(*index)++] = MakeHangarVertex(x0, y1, z1, nx, ny, nz, color);
    vertices[(*index)++] = MakeHangarVertex(x1, y1, z1, nx, ny, nz, color);
    vertices[(*index)++] = MakeHangarVertex(x0, y0, z0, nx, ny, nz, color);
    vertices[(*index)++] = MakeHangarVertex(x1, y1, z1, nx, ny, nz, color);
    vertices[(*index)++] = MakeHangarVertex(x1, y0, z0, nx, ny, nz, color);
}

static void AddHangarBox(TerrainVertex* vertices, int* index, float x, float z,
                         float halfWidth, float halfDepth, float height)
{
    const float y = Terrain_GetSurfaceHeight(x, z) + 0.02f;
    const float x0 = x - halfWidth, x1 = x + halfWidth;
    const float z0 = z - halfDepth, z1 = z + halfDepth;
    const DWORD wall = D3DCOLOR_XRGB(178, 181, 176);
    const DWORD roof = D3DCOLOR_XRGB(62, 74, 82);
    AddHangarQuad(vertices, index, x0, y + height, z0, x1, y + height, z1, 0.0f, 1.0f, 0.0f, roof);
    AddHangarQuad(vertices, index, x0, y, z0, x1, y + height, z0, 0.0f, 0.0f, -1.0f, wall);
    AddHangarQuad(vertices, index, x1, y, z0, x1, y + height, z1, 1.0f, 0.0f, 0.0f, wall);
    AddHangarQuad(vertices, index, x1, y, z1, x0, y + height, z1, 0.0f, 0.0f, 1.0f, wall);
    AddHangarQuad(vertices, index, x0, y, z1, x0, y + height, z0, -1.0f, 0.0f, 0.0f, wall);
}

static HRESULT CreateTerrain()
{
    D3DVERTEXELEMENT9 elements[5] = {
        { 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
        { 0, 24, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
        { 0, 28, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 }, D3DDECL_END()
    };
    ID3DXBuffer* vertexCode = NULL;
    ID3DXBuffer* pixelCode = NULL;
    ID3DXBuffer* errors = NULL;
    TerrainVertex* vertices = NULL;
    D3DVertexBuffer* lockedBuffer = NULL;
    int row, column, index = 0;
    HRESULT result;

    result = D3DXCompileShader(g_terrainVertexShaderSource, (UINT)strlen(g_terrainVertexShaderSource), NULL, NULL,
                               "main", "vs_2_0", 0, &vertexCode, &errors, NULL);
    if (FAILED(result)) goto fail;
    result = g_device->CreateVertexShader((DWORD*)vertexCode->GetBufferPointer(), &g_terrainVertexShader);
    if (FAILED(result)) goto fail;
    vertexCode->Release(); vertexCode = NULL;
    result = D3DXCompileShader(g_terrainPixelShaderSource, (UINT)strlen(g_terrainPixelShaderSource), NULL, NULL,
                               "main", "ps_2_0", 0, &pixelCode, &errors, NULL);
    if (FAILED(result)) goto fail;
    result = g_device->CreatePixelShader((DWORD*)pixelCode->GetBufferPointer(), &g_terrainPixelShader);
    if (FAILED(result)) goto fail;
    result = g_device->CreateVertexDeclaration(elements, &g_terrainDeclaration);
    if (FAILED(result)) goto fail;
    result = g_device->CreateVertexBuffer(sizeof(TerrainVertex) * TERRAIN_VERTEX_COUNT, D3DUSAGE_WRITEONLY,
                                          0, D3DPOOL_MANAGED, &g_terrainBuffer, NULL);
    if (FAILED(result)) goto fail;
    result = g_terrainBuffer->Lock(0, 0, (void**)&vertices, 0);
    if (FAILED(result)) goto fail;
    lockedBuffer = g_terrainBuffer;
    for (row = 0; row < TERRAIN_CELLS; ++row)
    {
        const float z0 = -TERRAIN_SIZE * 0.5f + row * TERRAIN_STEP;
        const float z1 = z0 + TERRAIN_STEP;
        for (column = 0; column < TERRAIN_CELLS; ++column)
        {
            const float x0 = -TERRAIN_SIZE * 0.5f + column * TERRAIN_STEP;
            const float x1 = x0 + TERRAIN_STEP;
            vertices[index++] = MakeTerrainVertex(x0, z0);
            vertices[index++] = MakeTerrainVertex(x0, z1);
            vertices[index++] = MakeTerrainVertex(x1, z1);
            vertices[index++] = MakeTerrainVertex(x0, z0);
            vertices[index++] = MakeTerrainVertex(x1, z1);
            vertices[index++] = MakeTerrainVertex(x1, z0);
        }
    }
    g_terrainBuffer->Unlock();
    vertices = NULL;
    lockedBuffer = NULL;

    result = g_device->CreateVertexBuffer(sizeof(TerrainVertex) * AIRPORT_VERTEX_COUNT, D3DUSAGE_WRITEONLY,
                                          0, D3DPOOL_MANAGED, &g_airportBuffer, NULL);
    if (FAILED(result)) goto fail;
    result = g_airportBuffer->Lock(0, 0, (void**)&vertices, 0);
    if (FAILED(result)) goto fail;
    lockedBuffer = g_airportBuffer;
    index = 0;
    AddAirportQuad(vertices, &index, -15.0f, -300.0f, 15.0f, 300.0f, 0.0f, 0.0f, 1.0f, 24.0f, D3DCOLOR_XRGB(220, 220, 220));
    AddAirportQuad(vertices, &index, 24.0f, -65.0f, 95.0f, 30.0f, 0.0f, 0.0f, 2.5f, 3.0f, D3DCOLOR_XRGB(200, 200, 200));
    g_airportBuffer->Unlock();
    vertices = NULL;
    lockedBuffer = NULL;

    result = g_device->CreateVertexBuffer(sizeof(TerrainVertex) * HANGAR_VERTEX_COUNT, D3DUSAGE_WRITEONLY,
                                          0, D3DPOOL_MANAGED, &g_hangarBuffer, NULL);
    if (FAILED(result)) goto fail;
    result = g_hangarBuffer->Lock(0, 0, (void**)&vertices, 0);
    if (FAILED(result)) goto fail;
    lockedBuffer = g_hangarBuffer;
    index = 0;
    AddHangarBox(vertices, &index, 58.0f, -30.0f, 13.0f, 18.0f, 10.0f);
    AddHangarBox(vertices, &index, 58.0f, 18.0f, 13.0f, 16.0f, 9.0f);
    AddHangarBox(vertices, &index, 88.0f, -8.0f, 9.0f, 12.0f, 7.0f);
    g_hangarBuffer->Unlock();
    vertices = NULL;
    lockedBuffer = NULL;

    result = LoadXboxTexture("game:\\assets\\terrain_grass.dds", &g_terrainGrassTexture);
    if (FAILED(result)) goto fail;
    result = LoadXboxTexture("game:\\assets\\runway_asphalt.dds", &g_runwayTexture);
    if (FAILED(result)) goto fail;

    if (vertexCode) vertexCode->Release();
    if (pixelCode) pixelCode->Release();
    if (errors) errors->Release();
    return S_OK;
fail:
    if (vertices && lockedBuffer) lockedBuffer->Unlock();
    if (errors) OutputDebugStringA((char*)errors->GetBufferPointer());
    if (vertexCode) vertexCode->Release();
    if (pixelCode) pixelCode->Release();
    if (errors) errors->Release();
    return result;
}

static float NormalizeStick(SHORT value)
{
    float normalized = (float)value / 32767.0f;
    if (normalized > -0.18f && normalized < 0.18f)
        return 0.0f;
    return normalized;
}

static void InitTimer()
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    g_secondsPerTick = 1.0f / (float)frequency.QuadPart;
    QueryPerformanceCounter(&g_lastTick);
}

static float GetDeltaTime()
{
    LARGE_INTEGER currentTick;
    QueryPerformanceCounter(&currentTick);
    const float deltaTime = (float)(currentTick.QuadPart - g_lastTick.QuadPart) * g_secondsPerTick;
    g_lastTick = currentTick;
    return deltaTime;
}

static float ClampCameraPitch(float value)
{
    if (value < -1.35f) return -1.35f;
    if (value > 1.35f) return 1.35f;
    return value;
}

static void SetFreeCameraFromChase(const FlightState* flight)
{
    const float forwardX = sinf(flight->yaw) * cosf(flight->pitch);
    const float forwardY = sinf(flight->pitch);
    const float forwardZ = -cosf(flight->yaw) * cosf(flight->pitch);
    g_freeCameraX = flight->x - forwardX * 18.0f;
    g_freeCameraY = flight->y - forwardY * 18.0f + 6.0f;
    g_freeCameraZ = flight->z - forwardZ * 18.0f;
    g_freeCameraYaw = flight->yaw;
    g_freeCameraPitch = -0.08f;
}

static void UpdateFreeCamera(float deltaTime, float moveX, float moveY, float lookX, float lookY)
{
    const float lookSpeed = 1.8f;
    const float moveSpeed = 70.0f;
    float forwardX;
    float forwardZ;
    float rightX;
    float rightZ;

    g_freeCameraYaw += lookX * lookSpeed * deltaTime;
    g_freeCameraPitch = ClampCameraPitch(g_freeCameraPitch + lookY * lookSpeed * deltaTime);
    forwardX = sinf(g_freeCameraYaw) * cosf(g_freeCameraPitch);
    forwardZ = -cosf(g_freeCameraYaw) * cosf(g_freeCameraPitch);
    rightX = cosf(g_freeCameraYaw);
    rightZ = sinf(g_freeCameraYaw);
    g_freeCameraX += (rightX * moveX + forwardX * moveY) * moveSpeed * deltaTime;
    g_freeCameraZ += (rightZ * moveX + forwardZ * moveY) * moveSpeed * deltaTime;
    if (g_freeCameraY < Terrain_GetSurfaceHeight(g_freeCameraX, g_freeCameraZ) + 2.0f)
        g_freeCameraY = Terrain_GetSurfaceHeight(g_freeCameraX, g_freeCameraZ) + 2.0f;
}

static void StartGameFromMenu()
{
    Flight_Reset();
    FlightScenario_Cancel(&g_flightScenario);
    g_flightScenario.telemetryEnabled = FALSE;
    g_freeCamera = FALSE;
    g_screen = GAME_PLAYING;
}

static void ActivateMenuSelection()
{
    if (g_screen == GAME_MAIN_MENU)
    {
        if (g_menuSelection == 0) StartGameFromMenu();
        else g_running = FALSE;
    }
    else if (g_menuSelection == 0)
    {
        g_screen = GAME_PLAYING;
    }
    else if (g_menuSelection == 1)
    {
        FlightScenario_Cancel(&g_flightScenario);
        g_flightScenario.telemetryEnabled = FALSE;
        g_freeCamera = FALSE;
        g_screen = GAME_MAIN_MENU;
        g_menuSelection = 0;
    }
    else
    {
        g_running = FALSE;
    }
}

static void UpdateMenuInput(const XINPUT_STATE* state)
{
    const int itemCount = g_screen == GAME_PAUSED ? 3 : 2;
    const BOOL upDown = (state->Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0;
    const BOOL downDown = (state->Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
    const BOOL stickUp = state->Gamepad.sThumbLY > 16384;
    const BOOL stickDown = state->Gamepad.sThumbLY < -16384;
    const BOOL confirmDown = (state->Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0;

    if ((upDown && !g_menuUpWasDown) || (stickUp && !g_menuStickUpWasDown))
        g_menuSelection = (g_menuSelection + itemCount - 1) % itemCount;
    if ((downDown && !g_menuDownWasDown) || (stickDown && !g_menuStickDownWasDown))
        g_menuSelection = (g_menuSelection + 1) % itemCount;
    if (confirmDown && !g_confirmWasDown)
        ActivateMenuSelection();
    g_menuUpWasDown = upDown;
    g_menuDownWasDown = downDown;
    g_menuStickUpWasDown = stickUp;
    g_menuStickDownWasDown = stickDown;
    g_confirmWasDown = confirmDown;
}

static void UpdateInput(float deltaTime)
{
    XINPUT_STATE state;
    FlightInput input = {0};
    const FlightState* flight = Flight_GetState();
    ZeroMemory(&state, sizeof(state));

    g_gamepadConnected = XInputGetState(0, &state) == ERROR_SUCCESS;
    if (g_gamepadConnected)
    {
        const BOOL startDown = (state.Gamepad.wButtons & XINPUT_GAMEPAD_START) != 0;
        const BOOL backDown = (state.Gamepad.wButtons & XINPUT_GAMEPAD_BACK) != 0;
        const BOOL dpadDown = (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
        const BOOL testButtonsDown = (state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0 &&
                                     (state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0;
        const float leftX = NormalizeStick(state.Gamepad.sThumbLX);
        const float leftY = NormalizeStick(state.Gamepad.sThumbLY);
        const float rightX = NormalizeStick(state.Gamepad.sThumbRX);
        const float rightY = NormalizeStick(state.Gamepad.sThumbRY);

        if (g_screen != GAME_PLAYING)
        {
            if (g_screen == GAME_PAUSED && startDown && !g_startWasDown)
                g_screen = GAME_PLAYING;
            else
                UpdateMenuInput(&state);
            g_startWasDown = startDown;
            return;
        }
        if (startDown && !g_startWasDown)
        {
            g_screen = GAME_PAUSED;
            g_menuSelection = 0;
            g_startWasDown = startDown;
            return;
        }
        g_startWasDown = startDown;

        if (backDown && !g_backWasDown)
        {
            g_freeCamera = !g_freeCamera;
            if (g_freeCamera) SetFreeCameraFromChase(flight);
        }
        if (dpadDown && !g_dpadDownWasDown)
            g_flightScenario.telemetryEnabled = !g_flightScenario.telemetryEnabled;
        if (testButtonsDown && !g_testButtonsWereDown)
            FlightScenario_Start(&g_flightScenario);
        g_backWasDown = backDown;
        g_dpadDownWasDown = dpadDown;
        g_testButtonsWereDown = testButtonsDown;

        // Chase camera: left stick controls the aircraft.  Free camera:
        // left stick flies the camera, right stick looks around.
        if (!g_freeCamera)
        {
            input.pitch = leftY;
            input.roll = leftX;
        }
        else
        {
            UpdateFreeCamera(deltaTime, leftX, leftY, rightX, rightY);
        }
        input.throttle = ((float)state.Gamepad.bRightTrigger - (float)state.Gamepad.bLeftTrigger) / 255.0f;
        input.yaw = ((state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) ? 1.0f : 0.0f) -
                    ((state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) ? 1.0f : 0.0f);

    }

    if (g_screen != GAME_PLAYING) return;
    if (FlightScenario_IsActive(&g_flightScenario))
        FlightScenario_BuildInput(&g_flightScenario, flight, &input);
    g_lastFlightInput = input;
    Flight_Step(&input, deltaTime);
    FlightScenario_Observe(&g_flightScenario, Flight_GetState(), deltaTime);
}

static void RenderTerrain(const XMMATRIX& view, const XMMATRIX& projection, const XMVECTOR& eye)
{
    const XMMATRIX world = XMMatrixIdentity();
    const XMMATRIX wvp = world * view * projection;
    XMVECTORF32 eyeComponents;
    float cameraPosition[4];
    const float sunDirection[4] = { -0.35f, 0.82f, -0.45f, 0.0f };
    const float fogColor[4] = { 0.32f, 0.66f, 0.86f, 1.0f };
    const float textured[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
    const float untextured[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    eyeComponents.v = eye;
    cameraPosition[0] = eyeComponents.f[0];
    cameraPosition[1] = eyeComponents.f[1];
    cameraPosition[2] = eyeComponents.f[2];
    cameraPosition[3] = 1.0f;

    g_device->SetVertexDeclaration(g_terrainDeclaration);
    g_device->SetVertexShader(g_terrainVertexShader);
    g_device->SetPixelShader(g_terrainPixelShader);
    g_device->SetVertexShaderConstantF(0, (float*)&wvp, 4);
    g_device->SetVertexShaderConstantF(4, cameraPosition, 1);
    g_device->SetVertexShaderConstantF(5, sunDirection, 1);
    g_device->SetPixelShaderConstantF(0, fogColor, 1);
    g_device->SetPixelShaderConstantF(1, textured, 1);
    g_device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    g_device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
    g_device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC);
    g_device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    g_device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
    g_device->SetSamplerState(0, D3DSAMP_MAXANISOTROPY, 16);
    g_device->SetStreamSource(0, g_terrainBuffer, 0, sizeof(TerrainVertex));
    g_device->SetTexture(0, g_terrainGrassTexture);
    g_device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, TERRAIN_VERTEX_COUNT / 3);
    g_device->SetStreamSource(0, g_airportBuffer, 0, sizeof(TerrainVertex));
    g_device->SetTexture(0, g_runwayTexture);
    g_device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, AIRPORT_VERTEX_COUNT / 3);
    g_device->SetPixelShaderConstantF(1, untextured, 1);
    g_device->SetTexture(0, g_terrainGrassTexture);
    g_device->SetStreamSource(0, g_hangarBuffer, 0, sizeof(TerrainVertex));
    g_device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, HANGAR_VERTEX_COUNT / 3);
}

static void RenderScene(const FlightState* flight, const XMMATRIX& view,
                        const XMMATRIX& projection, const XMVECTOR& eye)
{
    const XMMATRIX rotation = XMMatrixRotationRollPitchYaw(flight->pitch, -flight->yaw, flight->roll);
    const XMMATRIX translation = XMMatrixTranslation(flight->x, flight->y, flight->z);
    const XMMATRIX world = rotation * translation;
    const XMMATRIX wvp = world * view * projection;

    g_device->SetRenderState(D3DRS_ZENABLE, TRUE);
    g_device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    RenderTerrain(view, projection, eye);
    g_device->SetVertexDeclaration(g_planeDeclaration);
    g_device->SetStreamSource(0, g_planeBuffer, 0, GetPlaneVertexStride());
    g_device->SetVertexShader(g_planeVertexShader);
    g_device->SetPixelShader(g_planePixelShader);
    g_device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    g_device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
    g_device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC);
    g_device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    g_device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
    g_device->SetSamplerState(0, D3DSAMP_MAXANISOTROPY, 16);
    g_device->SetTexture(0, g_planeTexture);
    g_device->SetVertexShaderConstantF(0, (float*)&wvp, 4);
    g_device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, GetPlaneVertexCount() / 3);
}

static void RenderFrame()
{
    if (g_screen == GAME_MAIN_MENU)
    {
        g_device->SetRenderTarget(0, g_renderTarget);
        g_device->SetDepthStencilSurface(NULL);
        g_device->Clear(0, NULL, D3DCLEAR_TARGET, g_clearColor, 1.0f, 0);
        g_device->SetRenderState(D3DRS_ZENABLE, FALSE);
        g_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        RenderMenu();
        g_device->SynchronizeToPresentationInterval();
        g_device->Resolve(D3DRESOLVE_RENDERTARGET0, NULL, g_frontBuffer, NULL, 0, 0, NULL, 1.0f, 0, NULL);
        g_device->Swap(g_frontBuffer, NULL);
        return;
    }
    const FlightState* flight = Flight_GetState();
    const float forwardX = sinf(flight->yaw) * cosf(flight->pitch);
    const float forwardY = sinf(flight->pitch);
    const float forwardZ = -cosf(flight->yaw) * cosf(flight->pitch);
    XMVECTOR eye = {
        flight->x - forwardX * 18.0f,
        flight->y - forwardY * 18.0f + 6.0f,
        flight->z - forwardZ * 18.0f,
        0.0f
    };
    XMVECTOR target = {
        flight->x + forwardX * 8.0f,
        flight->y + forwardY * 8.0f + 1.2f,
        flight->z + forwardZ * 8.0f,
        0.0f
    };
    XMVECTOR up = { 0.0f, 1.0f, 0.0f, 0.0f };
    const XMMATRIX view = XMMatrixLookAtLH(eye, target, up);
    const XMMATRIX projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, 16.0f / 9.0f, 0.1f, 1200.0f);
    const D3DVECTOR4 clearColor = { 0.32f, 0.66f, 0.86f, 1.0f };
    const D3DRECT tileRects[3] = {
        { 0, 0, 1280, 256 },
        { 0, 256, 1280, 512 },
        { 0, 512, 1280, 720 }
    };
    const FLOAT postRectCorners[] = { -1.0f, 1.0f, 1.0f, 1.0f, -1.0f, -1.0f };

    if (g_freeCamera)
    {
        const float freeForwardX = sinf(g_freeCameraYaw) * cosf(g_freeCameraPitch);
        const float freeForwardY = sinf(g_freeCameraPitch);
        const float freeForwardZ = -cosf(g_freeCameraYaw) * cosf(g_freeCameraPitch);
        eye = XMVectorSet(g_freeCameraX, g_freeCameraY, g_freeCameraZ, 0.0f);
        target = XMVectorSet(g_freeCameraX + freeForwardX, g_freeCameraY + freeForwardY,
                             g_freeCameraZ + freeForwardZ, 0.0f);
    }

    g_device->SetRenderTarget(0, g_tiledMsaaEnabled ? g_tilingRenderTarget : g_renderTarget);
    g_device->SetRenderTarget(1, NULL);
    g_device->SetRenderTarget(2, NULL);
    g_device->SetRenderTarget(3, NULL);
    g_device->SetDepthStencilSurface(g_depthStencil);

    if (g_tiledMsaaEnabled)
    {
        g_device->BeginTiling(0, 3, tileRects, &clearColor, 1.0f, 0);
        g_device->BeginZPass(0);
        RenderScene(flight, view, projection, eye);
        g_device->EndZPass();
        g_device->EndTiling(D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_ALLFRAGMENTS |
                             D3DRESOLVE_CLEARRENDERTARGET | D3DRESOLVE_CLEARDEPTHSTENCIL,
                             NULL, g_sceneResolveTexture, &clearColor, 1.0f, 0, NULL);
    }
    else
    {
        g_device->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, g_clearColor, 1.0f, 0);
        RenderScene(flight, view, projection, eye);
        g_device->Resolve(D3DRESOLVE_RENDERTARGET0, NULL, g_sceneResolveTexture, NULL, 0, 0, NULL, 1.0f, 0, NULL);
    }

    /* Post pass is deliberately just a texture copy for now.  It gives tiled
       rendering a full-screen EDRAM target where HUD and later effects belong. */
    g_device->SetRenderTarget(0, g_renderTarget);
    g_device->SetDepthStencilSurface(NULL);
    g_device->SetRenderState(D3DRS_ZENABLE, FALSE);
    g_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    g_device->SetVertexDeclaration(g_postDeclaration);
    g_device->SetVertexShader(g_postVertexShader);
    g_device->SetPixelShader(g_postPixelShader);
    g_device->SetTexture(0, g_sceneResolveTexture);
    g_device->DrawPrimitiveUP(D3DPT_RECTLIST, 1, postRectCorners, 2 * sizeof(FLOAT));
    if (g_screen == GAME_PLAYING)
    {
        RenderHud(flight);
        RenderDevHud(flight);
    }
    else
    {
        RenderMenu();
    }
    g_device->SynchronizeToPresentationInterval();
    g_device->Resolve(D3DRESOLVE_RENDERTARGET0, NULL, g_frontBuffer, NULL, 0, 0, NULL, 1.0f, 0, NULL);
    g_device->Swap(g_frontBuffer, NULL);
}

static void DestroyRenderer()
{
    if (g_hudDeclaration) { g_hudDeclaration->Release(); g_hudDeclaration = NULL; }
    if (g_hudVertexShader) { g_hudVertexShader->Release(); g_hudVertexShader = NULL; }
    if (g_hudPixelShader) { g_hudPixelShader->Release(); g_hudPixelShader = NULL; }
    if (g_runwayTexture) { g_runwayTexture->Release(); g_runwayTexture = NULL; }
    if (g_terrainGrassTexture) { g_terrainGrassTexture->Release(); g_terrainGrassTexture = NULL; }
    if (g_planeTexture) { g_planeTexture->Release(); g_planeTexture = NULL; }
    if (g_sceneResolveTexture) { g_sceneResolveTexture->Release(); g_sceneResolveTexture = NULL; }
    if (g_frontBuffer) { g_frontBuffer->Release(); g_frontBuffer = NULL; }
    if (g_depthStencil) { g_depthStencil->Release(); g_depthStencil = NULL; }
    if (g_tilingRenderTarget) { g_tilingRenderTarget->Release(); g_tilingRenderTarget = NULL; }
    if (g_renderTarget) { g_renderTarget->Release(); g_renderTarget = NULL; }
    if (g_hangarBuffer) { g_hangarBuffer->Release(); g_hangarBuffer = NULL; }
    if (g_airportBuffer) { g_airportBuffer->Release(); g_airportBuffer = NULL; }
    if (g_terrainBuffer) { g_terrainBuffer->Release(); g_terrainBuffer = NULL; }
    if (g_terrainDeclaration) { g_terrainDeclaration->Release(); g_terrainDeclaration = NULL; }
    if (g_terrainVertexShader) { g_terrainVertexShader->Release(); g_terrainVertexShader = NULL; }
    if (g_terrainPixelShader) { g_terrainPixelShader->Release(); g_terrainPixelShader = NULL; }
    if (g_postDeclaration) { g_postDeclaration->Release(); g_postDeclaration = NULL; }
    if (g_postVertexShader) { g_postVertexShader->Release(); g_postVertexShader = NULL; }
    if (g_postPixelShader) { g_postPixelShader->Release(); g_postPixelShader = NULL; }
    if (g_planeBuffer) { g_planeBuffer->Release(); g_planeBuffer = NULL; }
    if (g_planeDeclaration) { g_planeDeclaration->Release(); g_planeDeclaration = NULL; }
    if (g_planeVertexShader) { g_planeVertexShader->Release(); g_planeVertexShader = NULL; }
    if (g_planePixelShader) { g_planePixelShader->Release(); g_planePixelShader = NULL; }
    if (g_device)
    {
        g_device->Release();
        g_device = NULL;
    }
    if (g_d3d)
    {
        g_d3d->Release();
        g_d3d = NULL;
    }
}

void __cdecl main()
{
    if (FAILED(CreateRenderer()))
        return;
    if (FAILED(CreateAircraft()))
    {
        OutputDebugStringA("Skyliner700: aircraft initialization failed.\n");
        DestroyRenderer();
        return;
    }
    if (FAILED(CreateTerrain()))
    {
        OutputDebugStringA("Skyliner700: terrain initialization failed.\n");
        DestroyRenderer();
        return;
    }
    if (FAILED(CreatePostProcess()))
    {
        DestroyRenderer();
        return;
    }
    if (FAILED(CreateHud()))
    {
        DestroyRenderer();
        return;
    }

    Flight_Init();
    FlightScenario_Init(&g_flightScenario);
    InitTimer();

    while (g_running)
    {
        UpdateInput(GetDeltaTime());
        RenderFrame();
    }

    DestroyRenderer();
}
