// d3d9_matrix.c — работа с матрицами мира и камеры

#include <d3d9.h>
#include "d3dx9_compat.h"
#include "renderer.h"

extern IDirect3DDevice9* GetD3D9Device();

static const D3DXVECTOR3 g_cameraEye = {22.0f, 12.0f, 285.0f};

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

    // На этапе тестовой ВПП камера смотрит на стартовую позицию самолёта.
    D3DXVECTOR3 at  = {0.0f, 0.2f, 150.0f};
    D3DXVECTOR3 up  = {0.0f, 1.0f,  0.0f};

    D3DXMATRIX matView;
    D3DXMatrixLookAtLH(&matView, &g_cameraEye, &at, &up);
    device->lpVtbl->SetTransform(device, D3DTS_VIEW, &matView);

    D3DXMATRIX matProj;
    float aspect = 800.0f / 600.0f; // TODO: вынести в конфиг
    D3DXMatrixPerspectiveFovLH(&matProj, D3DXToRadian(55.0f), aspect, 0.1f, 100.0f);
    device->lpVtbl->SetTransform(device, D3DTS_PROJECTION, &matProj);
}

void D3D9_SetAircraftWorldMatrix(float x, float y, float z, float pitch, float yaw, float roll) {
    IDirect3DDevice9* device = GetD3D9Device();
    D3DXMATRIX pitchMatrix, yawMatrix, rollMatrix, rotation, translation, temporary;
    if (!device) return;
    D3DXMatrixRotationX(&pitchMatrix, pitch);
    D3DXMatrixRotationY(&yawMatrix, yaw);
    D3DXMatrixRotationZ(&rollMatrix, roll);
    D3DXMatrixMultiply(&temporary, &pitchMatrix, &yawMatrix);
    D3DXMatrixMultiply(&rotation, &rollMatrix, &temporary);
    D3DXMatrixTranslation(&translation, x, y, z);
    D3DXMatrixMultiply(&temporary, &rotation, &translation);
    device->lpVtbl->SetTransform(device, D3DTS_WORLD, &temporary);
}

void D3D9_SetSkyWorldMatrix(void) {
    IDirect3DDevice9* device = GetD3D9Device();
    D3DXMATRIX matrix;
    if (!device) return;
    D3DXMatrixTranslation(&matrix, g_cameraEye.x, g_cameraEye.y, g_cameraEye.z);
    device->lpVtbl->SetTransform(device, D3DTS_WORLD, &matrix);
}

// Экспортируем через интерфейс
void Renderer_SetWorldMatrix(float x, float y, float z, float rotY) {
    D3D9_SetWorldMatrix(x, y, z, rotY);
}

void Renderer_SetAircraftWorldMatrix(float x, float y, float z, float pitch, float yaw, float roll) {
    D3D9_SetAircraftWorldMatrix(x, y, z, pitch, yaw, roll);
}

void Renderer_SetupCamera() {
    D3D9_SetupCamera();
}
