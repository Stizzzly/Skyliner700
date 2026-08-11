// main.c — высокоуровневая логика

#include <windows.h>
#include <stdio.h>
#include <psapi.h>
#include "core/window.h"
#include "render/renderer.h"
#include "model/plane.h"
#include "game/flight.h"
#include "game/camera.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Open console for diagnostics
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    FILE* __startup = fopen("C:\\Users\\ADMIN\\CLionProjects\\Skyliner700\\startup_marker.txt", "w"); if (__startup) { fprintf(__startup, "started: %llu\n", (unsigned long long)GetTickCount()); fclose(__startup); }

    if (!Window_Init(hInstance)) return 1;
    if (!Renderer_Init(Window_GetHWND())) return 1;
    Flight_Init();
    Camera_Init();
    Renderer_SetupCamera();

    // Загружаем модель
    if (!Renderer_CreateMesh(
        GetPlaneVertices(),
        GetPlaneVertexCount(),
        GetPlaneVertexStride(),
        GetPlaneFVF()
    )) {
        return 1;
    }

    // Игровой цикл with FPS and resource logging
    int frameCount = 0;
    DWORD fpsLast = GetTickCount();
    DWORD previousTime = GetTickCount();
    while (Window_IsRunning()) {
        Window_ProcessMessages();
        DWORD now = GetTickCount();
        float deltaTime = (now - previousTime) * 0.001f;
        previousTime = now;
        Flight_Update(deltaTime);
        const FlightState* flight = Flight_GetState();
        Camera_Update(deltaTime, flight);
        Renderer_BeginFrame();
        Renderer_RenderSky();
        Renderer_RenderTerrain();
        Renderer_SetAircraftWorldMatrix(flight->x, flight->y, flight->z,
                                        flight->pitch, flight->yaw, flight->roll);
        Renderer_RenderMesh();
        Renderer_EndFrame();

        frameCount++;
        now = GetTickCount();
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
