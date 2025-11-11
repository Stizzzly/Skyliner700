// main.c — высокоуровневая логика

#include <windows.h>
#include "core/window.h"
#include "renderer/renderer.h"
#include "model/plane.h"

static float planePosX = 0.0f;
static float planePosY = 0.0f;
static float planePosZ = 5.0f;
static float planeRotationY = 0.0f;

void UpdatePlane() {
    static DWORD lastTime = 0;
    DWORD currentTime = GetTickCount();
    float deltaTime = (currentTime - lastTime) * 0.001f;
    lastTime = currentTime;

    planeRotationY += 1.0f * deltaTime;
    planePosX = sinf(planeRotationY) * 4.0f;
    planePosZ = cosf(planeRotationY) * 4.0f + 5.0f;

    if (currentTime < 3000) {
        planePosY = (currentTime / 3000.0f) * 2.0f;
    } else {
        planePosY = 2.0f;
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    if (!Window_Init(hInstance)) return 1;
    if (!Renderer_Init(Window_GetHWND())) return 1;

    // Загружаем модель
    if (!Renderer_CreateMesh(
        GetPlaneVertices(),
        GetPlaneVertexCount(),
        GetPlaneVertexStride(),
        GetPlaneFVF()
    )) {
        return 1;
    }

    // Настройка камеры (один раз)
    Renderer_SetupCamera();

    // Игровой цикл
    while (Window_IsRunning()) {
        Window_ProcessMessages();
        UpdatePlane();
        Renderer_BeginFrame();
        Renderer_SetWorldMatrix(planePosX, planePosY, planePosZ, planeRotationY);
        Renderer_RenderMesh();
        Renderer_EndFrame();
    }

    Renderer_Shutdown();
    return 0;
}
