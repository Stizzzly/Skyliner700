#include <math.h>
#include <windows.h>
#include "core/window.h"
#include "render/renderer.h"
#include "camera.h"

static int g_thirdPerson = 1;
static int g_cameraKeyWasDown;
static int g_mouseCentered;
static float g_eyeX, g_eyeY, g_eyeZ;
static float g_freeYaw, g_freePitch;

static int KeyDown(int key) {
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}

static float Clamp(float value, float minimum, float maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static void SetThirdPersonCamera(const FlightState* flight, float interpolation) {
    const float forwardX = sinf(flight->yaw) * cosf(flight->pitch);
    const float forwardY = sinf(flight->pitch);
    const float forwardZ = -cosf(flight->yaw) * cosf(flight->pitch);
    const float desiredX = flight->x - forwardX * 18.0f;
    const float desiredY = flight->y - forwardY * 18.0f + 6.0f;
    const float desiredZ = flight->z - forwardZ * 18.0f;
    const float atX = flight->x + forwardX * 8.0f;
    const float atY = flight->y + forwardY * 8.0f + 1.2f;
    const float atZ = flight->z + forwardZ * 8.0f;
    const float cameraErrorX = desiredX - g_eyeX;
    const float cameraErrorY = desiredY - g_eyeY;
    const float cameraErrorZ = desiredZ - g_eyeZ;

    /* Do not let the follow camera lag kilometres behind a reset or a sharp
       manoeuvre.  Ordinary motion remains smoothed, but a large error snaps. */
    if (cameraErrorX * cameraErrorX + cameraErrorY * cameraErrorY + cameraErrorZ * cameraErrorZ > 900.0f) {
        g_eyeX = desiredX; g_eyeY = desiredY; g_eyeZ = desiredZ;
    } else {
        g_eyeX += cameraErrorX * interpolation;
        g_eyeY += cameraErrorY * interpolation;
        g_eyeZ += cameraErrorZ * interpolation;
    }
    Renderer_SetCameraLookAt(g_eyeX, g_eyeY, g_eyeZ, atX, atY, atZ);
}

static void CenterFreeCameraMouse(HWND window) {
    RECT rect;
    POINT point;
    GetClientRect(window, &rect);
    point.x = (rect.right - rect.left) / 2;
    point.y = (rect.bottom - rect.top) / 2;
    ClientToScreen(window, &point);
    SetCursorPos(point.x, point.y);
}

static void UpdateFreeCamera(float deltaTime) {
    HWND window = Window_GetHWND();
    POINT cursor, center;
    RECT rect;
    float forwardX, forwardY, forwardZ, rightX, rightZ;
    float moveX = 0.0f, moveY = 0.0f, moveZ = 0.0f;
    const float moveSpeed = 28.0f;

    if (GetForegroundWindow() == window) {
        GetClientRect(window, &rect);
        center.x = (rect.right - rect.left) / 2;
        center.y = (rect.bottom - rect.top) / 2;
        ClientToScreen(window, &center);
        if (g_mouseCentered) {
            GetCursorPos(&cursor);
            g_freeYaw += (cursor.x - center.x) * 0.0030f;
            g_freePitch -= (cursor.y - center.y) * 0.0030f;
        }
        g_mouseCentered = 1;
        g_freePitch = Clamp(g_freePitch, -1.35f, 1.35f);
        CenterFreeCameraMouse(window);
    }

    forwardX = sinf(g_freeYaw) * cosf(g_freePitch);
    forwardY = sinf(g_freePitch);
    forwardZ = cosf(g_freeYaw) * cosf(g_freePitch);
    rightX = cosf(g_freeYaw);
    rightZ = -sinf(g_freeYaw);
    if (KeyDown('W')) { moveX += forwardX; moveY += forwardY; moveZ += forwardZ; }
    if (KeyDown('S')) { moveX -= forwardX; moveY -= forwardY; moveZ -= forwardZ; }
    if (KeyDown('D')) { moveX += rightX; moveZ += rightZ; }
    if (KeyDown('A')) { moveX -= rightX; moveZ -= rightZ; }
    if (KeyDown(VK_PRIOR)) moveY += 1.0f;
    if (KeyDown(VK_NEXT)) moveY -= 1.0f;
    g_eyeX += moveX * moveSpeed * deltaTime;
    g_eyeY += moveY * moveSpeed * deltaTime;
    g_eyeZ += moveZ * moveSpeed * deltaTime;
    Renderer_SetCameraLookAt(g_eyeX, g_eyeY, g_eyeZ,
                             g_eyeX + forwardX, g_eyeY + forwardY, g_eyeZ + forwardZ);
}

void Camera_Init(void) {
    const FlightState* flight = Flight_GetState();
    g_eyeX = flight->x;
    g_eyeY = flight->y + 6.0f;
    g_eyeZ = flight->z + 18.0f;
    g_freeYaw = 3.14159265f;
    g_freePitch = -0.20f;
    SetThirdPersonCamera(flight, 1.0f);
}

void Camera_Update(float deltaTime, const FlightState* flight) {
    const int cameraKeyDown = KeyDown('C');
    if (cameraKeyDown && !g_cameraKeyWasDown) {
        g_thirdPerson = !g_thirdPerson;
        g_mouseCentered = 0;
        if (!g_thirdPerson) {
            const float forwardX = sinf(flight->yaw) * cosf(flight->pitch);
            const float forwardY = sinf(flight->pitch);
            const float forwardZ = -cosf(flight->yaw) * cosf(flight->pitch);
            g_freeYaw = atan2f(forwardX, forwardZ);
            g_freePitch = asinf(forwardY);
        }
    }
    g_cameraKeyWasDown = cameraKeyDown;

    if (g_thirdPerson) {
        SetThirdPersonCamera(flight, Clamp(deltaTime * 12.0f, 0.0f, 1.0f));
    } else {
        UpdateFreeCamera(deltaTime);
    }
}
