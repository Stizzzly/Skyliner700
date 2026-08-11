// main.c — высокоуровневая логика

#include <windows.h>
#include <stdio.h>
#include <psapi.h>
#include "core/window.h"
#include "render/renderer.h"
#include "model/plane.h"

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
        Renderer_BeginFrame();
        Renderer_RenderSky();
        Renderer_SetWorldMatrix(0.0f, 0.0f, 0.0f, 0.0f);
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
