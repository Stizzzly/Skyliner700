// main.c — высокоуровневая логика

#include <windows.h>
#include <stdio.h>
#include <math.h>
#define PSAPI_VERSION 1
#include <psapi.h>
#include "core/window.h"
#include "render/renderer.h"
#include "model/plane.h"
#include "game/flight.h"
#include "game/flight_input.h"
#include "game/flight_scenario.h"
#include "game/camera.h"

typedef enum {
    GAME_MAIN_MENU,
    GAME_PLAYING,
    GAME_PAUSED
} GameScreen;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Open console for diagnostics
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);

    if (!Window_Init(hInstance)) return 1;
    if (!Renderer_Init(Window_GetHWND())) return 1;
    Flight_Init();
    FlightScenario scenario;
    FlightScenario_Init(&scenario);
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
    GameScreen screen = GAME_MAIN_MENU;
    int menuSelection = 0;
    int quitRequested = 0;
    while (Window_IsRunning() && !quitRequested) {
        Window_ProcessMessages();
        if (!Window_IsRunning()) break;
        DWORD now = GetTickCount();
        float deltaTime = (now - previousTime) * 0.001f;
        previousTime = now;
        Flight_UpdateInputFrame();
        FlightInput input = {0};
        Flight_ReadKeyboardInput(&input);

        if (screen == GAME_PLAYING) {
            if (Flight_KeyPressed(VK_ESCAPE)) {
                screen = GAME_PAUSED;
                menuSelection = 0;
            } else {
                if (Flight_KeyPressed(VK_F5)) FlightScenario_Start(&scenario);
                if (Flight_KeyPressed(VK_F6)) scenario.telemetryEnabled = !scenario.telemetryEnabled;
                if (input.reset && FlightScenario_IsActive(&scenario)) FlightScenario_Cancel(&scenario);
                if (FlightScenario_IsActive(&scenario)) FlightScenario_BuildInput(&scenario, Flight_GetState(), &input);
                Flight_Step(&input, deltaTime);
                FlightScenario_Observe(&scenario, Flight_GetState(), deltaTime);
                Camera_Update(deltaTime, Flight_GetState());
            }
        } else {
            const int itemCount = screen == GAME_PAUSED ? 3 : 2;
            if (Flight_KeyPressed(VK_UP)) menuSelection = (menuSelection + itemCount - 1) % itemCount;
            if (Flight_KeyPressed(VK_DOWN)) menuSelection = (menuSelection + 1) % itemCount;
            if (screen == GAME_PAUSED && Flight_KeyPressed(VK_ESCAPE)) {
                screen = GAME_PLAYING;
            } else if (Flight_KeyPressed(VK_RETURN)) {
                if (screen == GAME_MAIN_MENU) {
                    if (menuSelection == 0) {
                        Flight_Reset();
                        FlightScenario_Cancel(&scenario);
                        scenario.telemetryEnabled = 0;
                        Camera_Init();
                        screen = GAME_PLAYING;
                    } else {
                        quitRequested = 1;
                    }
                } else if (menuSelection == 0) {
                    screen = GAME_PLAYING;
                } else if (menuSelection == 1) {
                    Flight_Reset();
                    FlightScenario_Cancel(&scenario);
                    scenario.telemetryEnabled = 0;
                    Camera_Init();
                    menuSelection = 0;
                    screen = GAME_MAIN_MENU;
                } else {
                    quitRequested = 1;
                }
            }
        }

        const FlightState* flight = Flight_GetState();
        Renderer_BeginFrame();
        Renderer_RenderSky();
        Renderer_RenderTerrain();
        /* The mesh nose is local -Z.  D3DX positive Y rotation turns local
           -Z toward world -X, so negate simulation yaw to point the rendered
           aircraft along the same forward vector used by flight and camera. */
        Renderer_SetAircraftWorldMatrix(flight->x, flight->y, flight->z,
                                        flight->pitch, -flight->yaw, flight->roll);
        Renderer_RenderMesh();
        float heading = fmodf(flight->yaw * 57.2957795f, 360.0f);
        if (heading < 0.0f) heading += 360.0f;
        if (screen == GAME_PLAYING) {
            Renderer_RenderHud(flight->speed * 3.6f,
                               flight->altitude,
                               flight->pitch * 57.2957795f, flight->roll * 57.2957795f, heading);
            Renderer_RenderDevHud(FlightScenario_Status(&scenario), scenario.telemetryEnabled,
                                  flight->throttle, flight->verticalSpeed, flight->lift, flight->drag,
                                  flight->onGround, input.pitch, input.roll, input.yaw);
        } else {
            Renderer_RenderMenu(screen == GAME_PAUSED, menuSelection);
        }
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
