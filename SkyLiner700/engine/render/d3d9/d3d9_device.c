// d3d9_device.c — инициализация устройства, очистка, вывод кадра

#include <d3d9.h>
#include <d3dx9.h>
#include "../renderer.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

static IDirect3D9*       g_pD3D = NULL;
static IDirect3DDevice9* g_pd3dDevice = NULL;

int D3D9_Init(HWND hWnd) {
    g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!g_pD3D) return 0;

    D3DPRESENT_PARAMETERS d3dpp = {0};
    d3dpp.Windowed               = TRUE;
    d3dpp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat       = D3DFMT_UNKNOWN;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    d3dpp.PresentationInterval   = D3DPRESENT_INTERVAL_IMMEDIATE;

    HRESULT hr = g_pD3D->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        hWnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &d3dpp,
        &g_pd3dDevice
    );

    if (FAILED(hr)) return 0;

    g_pd3dDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    g_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE);

    return 1;
}

void D3D9_Shutdown() {
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
    if (g_pD3D)       { g_pD3D->Release();       g_pD3D = NULL; }
}

void D3D9_BeginFrame() {
    if (!g_pd3dDevice) return;
    g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                        D3DCOLOR_XRGB(135, 206, 235), 1.0f, 0);
    g_pd3dDevice->BeginScene();
}

void D3D9_EndFrame() {
    if (!g_pd3dDevice) return;
    g_pd3dDevice->EndScene();
    g_pd3dDevice->Present(NULL, NULL, NULL, NULL);
}

// Экспортируем функции через интерфейс renderer.h
int Renderer_Init(HWND hWnd) { return D3D9_Init(hWnd); }
void Renderer_Shutdown() { D3D9_Shutdown(); }
void Renderer_BeginFrame() { D3D9_BeginFrame(); }
void Renderer_EndFrame() { D3D9_EndFrame(); }

IDirect3DDevice9* GetD3D9Device() { return g_pd3dDevice; } // для внутреннего использования в d3d9_*.c
