// Xbox 360 platform bootstrap for Skyliner 700.
// This deliberately contains no PC Win32 code and no runtime D3DX compiler.

#include "stdafx.h"
#include "game/flight.h"
#include "model/plane.h"

static Direct3D* g_d3d = NULL;
static D3DDevice* g_device = NULL;
static BOOL g_running = TRUE;
static BOOL g_gamepadConnected = FALSE;
static DWORD g_clearColor = D3DCOLOR_XRGB(82, 169, 220);
static LARGE_INTEGER g_lastTick;
static float g_secondsPerTick;
static D3DVertexBuffer* g_planeBuffer = NULL;
static D3DVertexDeclaration* g_planeDeclaration = NULL;
static D3DVertexShader* g_planeVertexShader = NULL;
static D3DPixelShader* g_planePixelShader = NULL;

static const char* g_vertexShaderSource =
"float4x4 WVP : register(c0);"
"struct IN { float4 position : POSITION; };"
"struct OUT { float4 position : POSITION; };"
"OUT main(IN input) { OUT output; output.position = mul(WVP, input.position); return output; }";

static const char* g_pixelShaderSource =
"float4 main() : COLOR { return float4(0.95, 0.97, 1.0, 1.0); }";

static HRESULT CreateRenderer()
{
    D3DPRESENT_PARAMETERS presentation;
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
    presentation.BackBufferWidth = min(videoMode.dwDisplayWidth, 1280);
    presentation.BackBufferHeight = min(videoMode.dwDisplayHeight, 720);
    presentation.BackBufferFormat = D3DFMT_X8R8G8B8;
    presentation.BackBufferCount = 1;
    presentation.EnableAutoDepthStencil = TRUE;
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
        DebugBreak();
        return E_FAIL;
    }

    OutputDebugStringA("Skyliner700: Xbox renderer initialized.\n");
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
    XMMATRIX projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, 16.0f / 9.0f, 0.1f, 100.0f);
    XMMATRIX wvp = world * view * projection;
    // Xbox 360 D3D does not use PC-style BeginScene/EndScene error returns:
    // they are documented no-ops. Clear and Present are command-buffer calls.
    g_device->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                    g_clearColor, 1.0f, 0);
    g_device->SetVertexDeclaration(g_planeDeclaration);
    g_device->SetStreamSource(0, g_planeBuffer, 0, GetPlaneVertexStride());
    g_device->SetVertexShader(g_planeVertexShader);
    g_device->SetPixelShader(g_planePixelShader);
    g_device->SetVertexShaderConstantF(0, (float*)&wvp, 4);
    g_device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, GetPlaneVertexCount() / 3);
    g_device->Present(NULL, NULL, NULL, NULL);
}

static void DestroyRenderer()
{
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

    Flight_Init();
    InitTimer();

    while (g_running)
    {
        UpdateInput(GetDeltaTime());
        RenderFrame();
    }

    DestroyRenderer();
}
