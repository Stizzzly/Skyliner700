// d3d9_device.c — инициализация устройства, очистка, вывод кадра

#include <d3d9.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "renderer.h"

#pragma comment(lib, "d3d9.lib")

static IDirect3D9*       g_pD3D = NULL;
static IDirect3DDevice9* g_pd3dDevice = NULL;
static IDirect3DTexture9* g_planeTexture = NULL;

void D3D9_Shutdown(void);

static int D3D9_LoadPlaneTexture(void) {
    char executablePath[MAX_PATH];
    char* fileName;
    HBITMAP bitmap;
    DIBSECTION dib;
    D3DLOCKED_RECT lockedRect;
    HRESULT hr;
    const int width = 512;
    const int height = 512;

    if (GetModuleFileNameA(NULL, executablePath, sizeof(executablePath)) == 0) {
        fprintf(stderr, "Unable to determine the executable path for plane texture.\n");
        return 0;
    }

    fileName = strrchr(executablePath, '\\');
    if (!fileName || (size_t)(fileName - executablePath) + sizeof("\\assets\\plane_livery.bmp") > sizeof(executablePath)) {
        fprintf(stderr, "Unable to build the plane texture path.\n");
        return 0;
    }
    strcpy(fileName, "\\assets\\plane_livery.bmp");

    bitmap = (HBITMAP)LoadImageA(NULL, executablePath, IMAGE_BITMAP, 0, 0,
                                 LR_LOADFROMFILE | LR_CREATEDIBSECTION);
    if (!bitmap) {
        fprintf(stderr, "Unable to load plane texture: %s (error %lu).\n", executablePath, GetLastError());
        return 0;
    }

    if (GetObject(bitmap, sizeof(dib), &dib) != sizeof(dib) ||
        dib.dsBm.bmWidth != width || abs(dib.dsBm.bmHeight) != height ||
        dib.dsBm.bmBitsPixel != 32 || !dib.dsBm.bmBits) {
        fprintf(stderr, "Plane texture must be a 512x512 32-bit BMP.\n");
        DeleteObject(bitmap);
        return 0;
    }

    hr = g_pd3dDevice->lpVtbl->CreateTexture(g_pd3dDevice, width, height, 1, 0,
                                              D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                                              &g_planeTexture, NULL);
    if (FAILED(hr)) {
        fprintf(stderr, "Unable to create D3D9 plane texture (0x%08lx).\n", (unsigned long)hr);
        DeleteObject(bitmap);
        return 0;
    }

    hr = g_planeTexture->lpVtbl->LockRect(g_planeTexture, 0, &lockedRect, NULL, 0);
    if (FAILED(hr)) {
        fprintf(stderr, "Unable to lock D3D9 plane texture (0x%08lx).\n", (unsigned long)hr);
        g_planeTexture->lpVtbl->Release(g_planeTexture);
        g_planeTexture = NULL;
        DeleteObject(bitmap);
        return 0;
    }

    for (int row = 0; row < height; ++row) {
        const int sourceRow = dib.dsBmih.biHeight > 0 ? height - 1 - row : row;
        const unsigned char* source = (const unsigned char*)dib.dsBm.bmBits + sourceRow * dib.dsBm.bmWidthBytes;
        unsigned char* destination = (unsigned char*)lockedRect.pBits + row * lockedRect.Pitch;
        memcpy(destination, source, (size_t)width * 4);
    }
    g_planeTexture->lpVtbl->UnlockRect(g_planeTexture, 0);
    DeleteObject(bitmap);
    return 1;
}

int D3D9_Init(HWND hWnd) {
    g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!g_pD3D) {
        FILE* __f = fopen("C:\\Users\\ADMIN\\CLionProjects\\Skyliner700\\debug_init.log","a"); if(__f){ fprintf(__f, "Direct3DCreate9 failed\n"); fclose(__f); }
        return 0;
    }

    D3DPRESENT_PARAMETERS d3dpp = {0};
    d3dpp.Windowed               = TRUE;
    d3dpp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat       = D3DFMT_UNKNOWN;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    d3dpp.PresentationInterval   = D3DPRESENT_INTERVAL_IMMEDIATE;

    HRESULT hr = g_pD3D->lpVtbl->CreateDevice(g_pD3D,
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        hWnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &d3dpp,
        &g_pd3dDevice
    );

    if (FAILED(hr)) {
        FILE* __f = fopen("C:\\Users\\ADMIN\\CLionProjects\\Skyliner700\\debug_init.log","a"); if(__f){ fprintf(__f, "CreateDevice failed: 0x%08x\n", (unsigned)hr); fclose(__f); }
        return 0;
    }

    g_pd3dDevice->lpVtbl->SetRenderState(g_pd3dDevice, D3DRS_LIGHTING, FALSE);
    g_pd3dDevice->lpVtbl->SetRenderState(g_pd3dDevice, D3DRS_CULLMODE, D3DCULL_NONE);
    g_pd3dDevice->lpVtbl->SetRenderState(g_pd3dDevice, D3DRS_ZENABLE, TRUE);
    g_pd3dDevice->lpVtbl->SetSamplerState(g_pd3dDevice, 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    g_pd3dDevice->lpVtbl->SetSamplerState(g_pd3dDevice, 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    g_pd3dDevice->lpVtbl->SetSamplerState(g_pd3dDevice, 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    g_pd3dDevice->lpVtbl->SetTextureStageState(g_pd3dDevice, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    g_pd3dDevice->lpVtbl->SetTextureStageState(g_pd3dDevice, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    g_pd3dDevice->lpVtbl->SetTextureStageState(g_pd3dDevice, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    g_pd3dDevice->lpVtbl->SetTextureStageState(g_pd3dDevice, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

    if (!D3D9_LoadPlaneTexture()) {
        D3D9_Shutdown();
        return 0;
    }

    return 1;
}

void D3D9_Shutdown() {
    if (g_planeTexture) { g_planeTexture->lpVtbl->Release(g_planeTexture); g_planeTexture = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->lpVtbl->Release(g_pd3dDevice); g_pd3dDevice = NULL; }
    if (g_pD3D)       { g_pD3D->lpVtbl->Release(g_pD3D);       g_pD3D = NULL; }
}

void D3D9_BeginFrame() {
    if (!g_pd3dDevice) return;
    g_pd3dDevice->lpVtbl->Clear(g_pd3dDevice, 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                        D3DCOLOR_XRGB(135, 206, 235), 1.0f, 0);
    g_pd3dDevice->lpVtbl->BeginScene(g_pd3dDevice);
}

void D3D9_EndFrame() {
    if (!g_pd3dDevice) return;
    g_pd3dDevice->lpVtbl->EndScene(g_pd3dDevice);
    g_pd3dDevice->lpVtbl->Present(g_pd3dDevice, NULL, NULL, NULL, NULL);
}

// Экспортируем функции через интерфейс renderer.h
int Renderer_Init(HWND hWnd) { return D3D9_Init(hWnd); }
void Renderer_Shutdown() { D3D9_Shutdown(); }
void Renderer_BeginFrame() { D3D9_BeginFrame(); }
void Renderer_EndFrame() { D3D9_EndFrame(); }

IDirect3DDevice9* GetD3D9Device() { return g_pd3dDevice; } // для внутреннего использования в d3d9_*.c
IDirect3DBaseTexture9* GetPlaneTexture() { return (IDirect3DBaseTexture9*)g_planeTexture; }
