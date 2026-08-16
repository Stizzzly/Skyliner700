// Xbox 360 platform bootstrap for Skyliner 700.
// This deliberately contains no PC Win32 code and no runtime D3DX compiler.

#include "stdafx.h"
#include "game/flight.h"
#include "model/plane.h"
#include "world/terrain.h"

#define TERRAIN_CELLS 64
#define TERRAIN_SIZE 1200.0f
#define TERRAIN_STEP (TERRAIN_SIZE / TERRAIN_CELLS)
#define TERRAIN_VERTEX_COUNT (TERRAIN_CELLS * TERRAIN_CELLS * 6)
#define AIRPORT_VERTEX_COUNT 12

typedef struct TerrainVertex
{
    float x, y, z;
    float nx, ny, nz;
    DWORD color;
} TerrainVertex;

static Direct3D* g_d3d = NULL;
static D3DDevice* g_device = NULL;
static BOOL g_running = TRUE;
static BOOL g_gamepadConnected = FALSE;
static DWORD g_clearColor = D3DCOLOR_XRGB(82, 169, 220);
static UINT g_renderWidth = 0;
static UINT g_renderHeight = 0;
static LARGE_INTEGER g_lastTick;
static float g_secondsPerTick;
static D3DSurface* g_renderTarget = NULL;
static D3DSurface* g_depthStencil = NULL;
static D3DTexture* g_frontBuffer = NULL;
static D3DVertexBuffer* g_planeBuffer = NULL;
static D3DVertexDeclaration* g_planeDeclaration = NULL;
static D3DVertexShader* g_planeVertexShader = NULL;
static D3DPixelShader* g_planePixelShader = NULL;
static D3DVertexBuffer* g_terrainBuffer = NULL;
static D3DVertexBuffer* g_airportBuffer = NULL;
static D3DVertexDeclaration* g_terrainDeclaration = NULL;
static D3DVertexShader* g_terrainVertexShader = NULL;
static D3DPixelShader* g_terrainPixelShader = NULL;

static const char* g_vertexShaderSource =
"float4x4 WVP : register(c0);"
"struct IN { float4 position : POSITION; };"
"struct OUT { float4 position : POSITION; };"
"OUT main(IN input) { OUT output; output.position = mul(WVP, input.position); return output; }";

static const char* g_pixelShaderSource =
"float4 main() : COLOR { return float4(0.95, 0.97, 1.0, 1.0); }";

static const char* g_terrainVertexShaderSource =
"float4x4 WVP : register(c0);"
"float4 CameraPosition : register(c4);"
"float4 SunDirection : register(c5);"
"struct IN { float4 position : POSITION; float3 normal : NORMAL; float4 color : COLOR0; };"
"struct OUT { float4 position : POSITION; float4 color : COLOR0; float fog : TEXCOORD0; };"
"OUT main(IN input) {"
" OUT output; float light = 0.48 + 0.52 * saturate(dot(input.normal, SunDirection.xyz));"
" float distanceToCamera = length(CameraPosition.xyz - input.position.xyz);"
" output.position = mul(WVP, input.position); output.color = input.color * light;"
" output.fog = saturate((distanceToCamera - 360.0) / 760.0); return output; }";

static const char* g_terrainPixelShaderSource =
"float4 FogColor : register(c0);"
"struct IN { float4 color : COLOR0; float fog : TEXCOORD0; };"
"float4 main(IN input) : COLOR { return lerp(input.color, FogColor, input.fog); }";

static HRESULT CreateRenderer()
{
    D3DPRESENT_PARAMETERS presentation;
    D3DSURFACE_PARAMETERS surfaceParameters;
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
                                 D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                 &presentation, &g_device);
    if (FAILED(result))
    {
        OutputDebugStringA("Skyliner700: CreateDevice failed.\n");
        return E_FAIL;
    }

    /* This explicit target/front-buffer chain is the foundation for the
       upcoming EDRAM tiled MSAA path.  It is the XDK pattern: draw into EDRAM,
       resolve into UMA memory, then Swap the resolved texture. */
    ZeroMemory(&surfaceParameters, sizeof(surfaceParameters));
    result = g_device->CreateRenderTarget(g_renderWidth, g_renderHeight, D3DFMT_X8R8G8B8,
                                          D3DMULTISAMPLE_NONE, 0, FALSE,
                                          &g_renderTarget, &surfaceParameters);
    if (FAILED(result)) return result;
    surfaceParameters.Base = GPU_EDRAM_TILES / 2;
    result = g_device->CreateDepthStencilSurface(g_renderWidth, g_renderHeight, D3DFMT_D24S8,
                                                  D3DMULTISAMPLE_NONE, 0, FALSE,
                                                  &g_depthStencil, &surfaceParameters);
    if (FAILED(result)) return result;
    result = g_device->CreateTexture(g_renderWidth, g_renderHeight, 1, 0,
                                     presentation.FrontBufferFormat, D3DPOOL_DEFAULT,
                                     &g_frontBuffer, NULL);
    if (FAILED(result)) return result;

    OutputDebugStringA("Skyliner700: Xbox explicit EDRAM resolve path initialized.\n");
    return S_OK;
}

static HRESULT CreateAircraft()
{
    D3DVERTEXELEMENT9 elements[2] = {
        { 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 }, D3DDECL_END()
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
    return vertex;
}

static TerrainVertex MakeAirportVertex(float x, float z, DWORD color)
{
    TerrainVertex vertex;
    vertex.x = x;
    vertex.y = Terrain_GetSurfaceHeight(x, z) + 0.01f;
    vertex.z = z;
    vertex.nx = 0.0f;
    vertex.ny = 1.0f;
    vertex.nz = 0.0f;
    vertex.color = color;
    return vertex;
}

static void AddAirportQuad(TerrainVertex* vertices, int* index,
                           float x0, float z0, float x1, float z1, DWORD color)
{
    vertices[(*index)++] = MakeAirportVertex(x0, z0, color);
    vertices[(*index)++] = MakeAirportVertex(x0, z1, color);
    vertices[(*index)++] = MakeAirportVertex(x1, z1, color);
    vertices[(*index)++] = MakeAirportVertex(x0, z0, color);
    vertices[(*index)++] = MakeAirportVertex(x1, z1, color);
    vertices[(*index)++] = MakeAirportVertex(x1, z0, color);
}

static HRESULT CreateTerrain()
{
    D3DVERTEXELEMENT9 elements[4] = {
        { 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
        { 0, 24, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 }, D3DDECL_END()
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
    AddAirportQuad(vertices, &index, -15.0f, -300.0f, 15.0f, 300.0f, D3DCOLOR_XRGB(64, 68, 72));
    AddAirportQuad(vertices, &index, 24.0f, -65.0f, 95.0f, 30.0f, D3DCOLOR_XRGB(71, 75, 78));
    g_airportBuffer->Unlock();
    vertices = NULL;
    lockedBuffer = NULL;

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

static void UpdateInput(float deltaTime)
{
    XINPUT_STATE state;
    FlightInput input = {0};
    ZeroMemory(&state, sizeof(state));

    g_gamepadConnected = XInputGetState(0, &state) == ERROR_SUCCESS;
    if (g_gamepadConnected)
    {
        // Left stick: pitch and roll. Triggers: throttle. Bumpers: yaw.
        input.pitch = NormalizeStick(state.Gamepad.sThumbLY);
        input.roll = NormalizeStick(state.Gamepad.sThumbLX);
        input.throttle = ((float)state.Gamepad.bRightTrigger - (float)state.Gamepad.bLeftTrigger) / 255.0f;
        input.yaw = ((state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) ? 1.0f : 0.0f) -
                    ((state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) ? 1.0f : 0.0f);
        input.reset = (state.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0;

        if (state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
            g_running = FALSE;
    }

    Flight_Step(&input, deltaTime);
}

static void RenderTerrain(const XMMATRIX& view, const XMMATRIX& projection, const XMVECTOR& eye)
{
    const XMMATRIX world = XMMatrixIdentity();
    const XMMATRIX wvp = world * view * projection;
    XMVECTORF32 eyeComponents;
    float cameraPosition[4];
    const float sunDirection[4] = { -0.35f, 0.82f, -0.45f, 0.0f };
    const float fogColor[4] = { 0.32f, 0.66f, 0.86f, 1.0f };

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
    g_device->SetStreamSource(0, g_terrainBuffer, 0, sizeof(TerrainVertex));
    g_device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, TERRAIN_VERTEX_COUNT / 3);
    g_device->SetStreamSource(0, g_airportBuffer, 0, sizeof(TerrainVertex));
    g_device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, AIRPORT_VERTEX_COUNT / 3);
}

static void RenderFrame()
{
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
    XMMATRIX rotation = XMMatrixRotationRollPitchYaw(flight->pitch, -flight->yaw, flight->roll);
    XMMATRIX translation = XMMatrixTranslation(flight->x, flight->y, flight->z);
    XMMATRIX world = rotation * translation;
    XMMATRIX view = XMMatrixLookAtLH(eye, target, up);
    XMMATRIX projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, 16.0f / 9.0f, 0.1f, 1200.0f);
    XMMATRIX wvp = world * view * projection;
    // Xbox 360 D3D uses command-buffer operations rather than PC-style
    // BeginScene/EndScene error returns; this frame ends in Resolve + Swap.
    g_device->SetRenderTarget(0, g_renderTarget);
    g_device->SetRenderTarget(1, NULL);
    g_device->SetRenderTarget(2, NULL);
    g_device->SetRenderTarget(3, NULL);
    g_device->SetDepthStencilSurface(g_depthStencil);
    g_device->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                    g_clearColor, 1.0f, 0);
    RenderTerrain(view, projection, eye);
    g_device->SetVertexDeclaration(g_planeDeclaration);
    g_device->SetStreamSource(0, g_planeBuffer, 0, GetPlaneVertexStride());
    g_device->SetVertexShader(g_planeVertexShader);
    g_device->SetPixelShader(g_planePixelShader);
    g_device->SetVertexShaderConstantF(0, (float*)&wvp, 4);
    g_device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, GetPlaneVertexCount() / 3);
    g_device->SynchronizeToPresentationInterval();
    g_device->Resolve(D3DRESOLVE_RENDERTARGET0, NULL, g_frontBuffer, NULL, 0, 0, NULL, 1.0f, 0, NULL);
    g_device->Swap(g_frontBuffer, NULL);
}

static void DestroyRenderer()
{
    if (g_frontBuffer) { g_frontBuffer->Release(); g_frontBuffer = NULL; }
    if (g_depthStencil) { g_depthStencil->Release(); g_depthStencil = NULL; }
    if (g_renderTarget) { g_renderTarget->Release(); g_renderTarget = NULL; }
    if (g_airportBuffer) { g_airportBuffer->Release(); g_airportBuffer = NULL; }
    if (g_terrainBuffer) { g_terrainBuffer->Release(); g_terrainBuffer = NULL; }
    if (g_terrainDeclaration) { g_terrainDeclaration->Release(); g_terrainDeclaration = NULL; }
    if (g_terrainVertexShader) { g_terrainVertexShader->Release(); g_terrainVertexShader = NULL; }
    if (g_terrainPixelShader) { g_terrainPixelShader->Release(); g_terrainPixelShader = NULL; }
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
        DestroyRenderer();
        return;
    }
    if (FAILED(CreateTerrain()))
    {
        DestroyRenderer();
        return;
    }

    Flight_Init();
    InitTimer();

    while (g_running)
    {
        UpdateInput(GetDeltaTime());
        RenderFrame();
    }

    DestroyRenderer();
}
