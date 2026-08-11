#include <math.h>
#include <windows.h>
#include "flight.h"

#define GROUND_CLEARANCE 0.45f
#define STALL_SPEED 18.0f

static FlightState g_flight;
static float g_velocityX;
static float g_velocityY;
static float g_velocityZ;

static int KeyDown(int key) {
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}

void Flight_Reset(void) {
    g_flight.x = 0.0f;
    g_flight.y = GROUND_CLEARANCE;
    g_flight.z = 220.0f;
    g_flight.pitch = 0.0f;
    g_flight.yaw = 0.0f;
    g_flight.roll = 0.0f;
    g_flight.speed = 0.0f;
    g_flight.throttle = 0.0f;
    g_flight.onGround = 1;
    g_velocityX = g_velocityY = g_velocityZ = 0.0f;
}

void Flight_Init(void) {
    Flight_Reset();
}

void Flight_Update(float deltaTime) {
    const float pitchInput = (KeyDown(VK_UP) ? 1.0f : 0.0f) - (KeyDown(VK_DOWN) ? 1.0f : 0.0f);
    const float rollInput = (KeyDown(VK_RIGHT) ? 1.0f : 0.0f) - (KeyDown(VK_LEFT) ? 1.0f : 0.0f);
    const float yawInput = (KeyDown('E') ? 1.0f : 0.0f) - (KeyDown('Q') ? 1.0f : 0.0f);
    const float throttleInput = (KeyDown(VK_SHIFT) ? 1.0f : 0.0f) - (KeyDown(VK_CONTROL) ? 1.0f : 0.0f);
    float forwardX, forwardY, forwardZ;
    float horizontalSpeed;

    if (KeyDown('R')) Flight_Reset();
    if (deltaTime > 0.05f) deltaTime = 0.05f;

    g_flight.throttle += throttleInput * 0.35f * deltaTime;
    if (g_flight.throttle < 0.0f) g_flight.throttle = 0.0f;
    if (g_flight.throttle > 1.0f) g_flight.throttle = 1.0f;

    g_flight.pitch += pitchInput * 0.75f * deltaTime;
    g_flight.roll += rollInput * 0.95f * deltaTime;
    g_flight.yaw += yawInput * 0.55f * deltaTime;
    if (g_flight.pitch > 0.55f) g_flight.pitch = 0.55f;
    if (g_flight.pitch < -0.35f) g_flight.pitch = -0.35f;
    if (g_flight.roll > 0.85f) g_flight.roll = 0.85f;
    if (g_flight.roll < -0.85f) g_flight.roll = -0.85f;
    if (rollInput == 0.0f) g_flight.roll *= 1.0f - 1.8f * deltaTime;

    forwardX = sinf(g_flight.yaw) * cosf(g_flight.pitch);
    forwardY = sinf(g_flight.pitch);
    forwardZ = -cosf(g_flight.yaw) * cosf(g_flight.pitch);

    g_velocityX += forwardX * (22.0f * g_flight.throttle) * deltaTime;
    g_velocityY += forwardY * (22.0f * g_flight.throttle) * deltaTime;
    g_velocityZ += forwardZ * (22.0f * g_flight.throttle) * deltaTime;

    horizontalSpeed = sqrtf(g_velocityX * g_velocityX + g_velocityZ * g_velocityZ);
    if (!g_flight.onGround || horizontalSpeed > STALL_SPEED) {
        const float lift = horizontalSpeed > STALL_SPEED
                         ? (horizontalSpeed - STALL_SPEED) * (horizontalSpeed - STALL_SPEED) * 0.020f * cosf(g_flight.roll)
                         : 0.0f;
        g_velocityY += (lift - 9.81f) * deltaTime;
    }

    g_velocityX *= 1.0f - 0.0035f * horizontalSpeed * deltaTime;
    g_velocityZ *= 1.0f - 0.0035f * horizontalSpeed * deltaTime;
    g_velocityY *= 1.0f - 0.018f * deltaTime;
    g_flight.x += g_velocityX * deltaTime;
    g_flight.y += g_velocityY * deltaTime;
    g_flight.z += g_velocityZ * deltaTime;

    if (g_flight.y <= GROUND_CLEARANCE) {
        g_flight.y = GROUND_CLEARANCE;
        if (g_velocityY < 0.0f) g_velocityY = 0.0f;
        g_flight.onGround = 1;
        g_velocityX *= 1.0f - 0.8f * deltaTime;
        g_velocityZ *= 1.0f - 0.8f * deltaTime;
    } else {
        g_flight.onGround = 0;
    }

    g_flight.speed = sqrtf(g_velocityX * g_velocityX + g_velocityY * g_velocityY + g_velocityZ * g_velocityZ);
}

const FlightState* Flight_GetState(void) {
    return &g_flight;
}
