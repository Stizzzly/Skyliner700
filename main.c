// main.c — высокоуровневая логика

#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <psapi.h>
#include "core/window.h"
#include "render/renderer.h"
#include "model/plane.h"

static float planePosX = 0.0f;
static float planePosY = 0.0f;
static float planePosZ = 5.0f;
static float planeRotationY = 0.0f;

void UpdatePlane() {
    static DWORD lastTime = 0;
    DWORD currentTime = GetTickCount();
    if (lastTime == 0) lastTime = currentTime;
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
    // Open console for diagnostics
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    FILE* __startup = fopen("C:\\Users\\ADMIN\\CLionProjects\\Skyliner700\\startup_marker.txt", "w"); if (__startup) { fprintf(__startup, "started: %llu\n", (unsigned long long)GetTickCount()); fclose(__startup); }

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

    // Игровой цикл with FPS and resource logging
    int frameCount = 0;
    DWORD fpsLast = GetTickCount();
    while (Window_IsRunning()) {
        Window_ProcessMessages();
        UpdatePlane();
        Renderer_BeginFrame();
        Renderer_SetWorldMatrix(planePosX, planePosY, planePosZ, planeRotationY);
        Renderer_RenderMesh();
        Renderer_EndFrame();

        frameCount++;
        DWORD now = GetTickCount();
        if (now - fpsLast >= 1000) {
            // FPS
            printf("FPS: %d\n", frameCount);
            frameCount = 0;
            fpsLast = now;

            // Memory usage
            PROCESS_MEMORY_COUNTERS pmc;
            if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
                printf("WorkingSetSize: %zu KB, PagefileUsage: %zu KB\n",
                       (size_t)(pmc.WorkingSetSize/1024), (size_t)(pmc.PagefileUsage/1024));
            }
            fflush(stdout);
        }
    }

    Renderer_Shutdown();
    return 0;
}
