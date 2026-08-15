// Xbox 360 platform bootstrap for Skyliner 700.
// This deliberately contains no PC Win32 code and no runtime D3DX compiler.

#include "stdafx.h"

static Direct3D* g_d3d = NULL;
static D3DDevice* g_device = NULL;
static BOOL g_running = TRUE;
static BOOL g_gamepadConnected = FALSE;
static DWORD g_clearColor = D3DCOLOR_XRGB(82, 169, 220);

static HRESULT CreateRenderer()
{
    D3DPRESENT_PARAMETERS presentation;
    XVIDEO_MODE videoMode;

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

    if (FAILED(g_d3d->CreateDevice(0, D3DDEVTYPE_HAL, NULL,
                                    D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                    &presentation, &g_device)))
    {
        OutputDebugStringA("Skyliner700: CreateDevice failed.\n");
        return E_FAIL;
    }

    OutputDebugStringA("Skyliner700: Xbox renderer initialized.\n");
    return S_OK;
}

static void UpdateInput()
{
    XINPUT_STATE state;
    ZeroMemory(&state, sizeof(state));

    g_gamepadConnected = XInputGetState(0, &state) == ERROR_SUCCESS;
    if (!g_gamepadConnected)
    {
        g_clearColor = D3DCOLOR_XRGB(70, 100, 130);
        return;
    }

    // Temporary visual feedback while input mapping is being ported.
    // A = runway orange, B = terrain green, default = sky blue.
    if (state.Gamepad.wButtons & XINPUT_GAMEPAD_A)
        g_clearColor = D3DCOLOR_XRGB(220, 135, 55);
    else if (state.Gamepad.wButtons & XINPUT_GAMEPAD_B)
        g_clearColor = D3DCOLOR_XRGB(66, 145, 75);
    else
        g_clearColor = D3DCOLOR_XRGB(82, 169, 220);

    if (state.Gamepad.wButtons & XINPUT_GAMEPAD_START)
        g_running = FALSE;
}

static BOOL RenderFrame()
{
    HRESULT result = g_device->BeginScene();
    if (FAILED(result))
    {
        OutputDebugStringA("Skyliner700: BeginScene failed.\n");
        return FALSE;
    }

    result = g_device->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                             g_clearColor, 1.0f, 0);
    if (SUCCEEDED(result))
        result = g_device->EndScene();
    else
        g_device->EndScene();

    if (FAILED(result))
    {
        OutputDebugStringA("Skyliner700: frame clear failed.\n");
        return FALSE;
    }

    result = g_device->Present(NULL, NULL, NULL, NULL);
    if (FAILED(result))
    {
        OutputDebugStringA("Skyliner700: Present failed.\n");
        return FALSE;
    }

    return TRUE;
}

static void DestroyRenderer()
{
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

    while (g_running)
    {
        UpdateInput();
        if (!RenderFrame())
            break;
    }

    DestroyRenderer();
}
