// d3d9_matrix.c — работа с матрицами мира и камеры

#include <d3d9.h>
#include "d3dx9_compat.h"
#include "renderer.h"

extern IDirect3DDevice9* GetD3D9Device();

void D3D9_SetWorldMatrix(float x, float y, float z, float rotY) {
    IDirect3DDevice9* device = GetD3D9Device();
    if (!device) return;

    D3DXMATRIX matWorld, matRotate, matTranslate, tmp;
    D3DXMatrixRotationY(&matRotate, rotY);
    D3DXMatrixTranslation(&matTranslate, x, y, z);
    D3DXMatrixMultiply(&tmp, &matRotate, &matTranslate);
    matWorld = tmp;

    device->lpVtbl->SetTransform(device, D3DTS_WORLD, &matWorld);
}

void D3D9_SetupCamera() {
    IDirect3DDevice9* device = GetD3D9Device();
    if (!device) return;

    D3DXVECTOR3 eye = {0.0f, 3.0f, -7.0f};
    D3DXVECTOR3 at  = {0.0f, 1.0f,  0.0f};
    D3DXVECTOR3 up  = {0.0f, 1.0f,  0.0f};

    D3DXMATRIX matView;
    D3DXMatrixLookAtLH(&matView, &eye, &at, &up);
    device->lpVtbl->SetTransform(device, D3DTS_VIEW, &matView);

    D3DXMATRIX matProj;
    float aspect = 800.0f / 600.0f; // TODO: вынести в конфиг
    D3DXMatrixPerspectiveFovLH(&matProj, D3DXToRadian(60.0f), aspect, 0.1f, 100.0f);
    device->lpVtbl->SetTransform(device, D3DTS_PROJECTION, &matProj);
}

// Экспортируем через интерфейс
void Renderer_SetWorldMatrix(float x, float y, float z, float rotY) {
    D3D9_SetWorldMatrix(x, y, z, rotY);
}

void Renderer_SetupCamera() {
    D3D9_SetupCamera();
}
