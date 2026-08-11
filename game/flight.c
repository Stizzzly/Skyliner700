#include <math.h>
#include <windows.h>
#include "flight.h"
#include "world/terrain.h"

#define AIRCRAFT_CLEARANCE 0.53f
#define STALL_SPEED 18.0f
#define ENGINE_ACCELERATION 18.0f
#define ENGINE_FULL_POWER_SPEED 82.0f
#define MAX_AIRSPEED 90.0f
#define PARASITIC_DRAG 0.0026f

static FlightState g_flight;
static float g_velocityX;
static float g_velocityY;
static float g_velocityZ;

static int KeyDown(int key) {
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}

static float Clamp(float value, float minimum, float maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static float AircraftSurfaceHeight(float x, float z, float yaw) {
    /* Sample the footprint (nose, tail and wingtips), rather than just the
       centre point. This stops a wing or fuselage cutting into a hillside. */
    static const float contacts[][2] = {{0.0f,-3.20f},{0.0f,3.15f},{-3.95f,0.55f},{3.95f,0.55f},{0.0f,0.0f}};
    float highest = -100000.0f;
    const float cosine = cosf(yaw);
    const float sine = sinf(yaw);
    for (int point = 0; point < (int)(sizeof(contacts) / sizeof(contacts[0])); ++point) {
        const float localX = contacts[point][0];
        const float localZ = contacts[point][1];
        const float worldX = x + localX * cosine + localZ * sine;
        const float worldZ = z - localX * sine + localZ * cosine;
        const float height = Terrain_GetSurfaceHeight(worldX, worldZ);
        if (height > highest) highest = height;
    }
    return highest;
}

static float AircraftClearanceForAttitude(float pitch, float roll) {
    /* The model's fuselage hangs 0.45 m below its origin. Pitch moves the
       tail and roll moves a wing lower, so compensate before mesh contact. */
    return AIRCRAFT_CLEARANCE + fabsf(sinf(pitch)) * 2.8f + fabsf(sinf(roll)) * 3.6f;
}

void Flight_Reset(void) {
    g_flight.x = 0.0f;
    g_flight.z = 220.0f;
    g_flight.y = AircraftSurfaceHeight(g_flight.x, g_flight.z, 0.0f) + AIRCRAFT_CLEARANCE;
    g_flight.pitch = 0.0f;
    g_flight.yaw = 0.0f;
    g_flight.roll = 0.0f;
    g_flight.speed = 0.0f;
    g_flight.throttle = 0.0f;
    g_flight.altitude = 0.0f;
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
    float forwardSpeed, airspeed, lift = 0.0f;

    if (KeyDown('R')) Flight_Reset();
    if (deltaTime > 0.05f) deltaTime = 0.05f;

    g_flight.throttle += throttleInput * 0.35f * deltaTime;
    if (g_flight.throttle < 0.0f) g_flight.throttle = 0.0f;
    if (g_flight.throttle > 1.0f) g_flight.throttle = 1.0f;

    g_flight.pitch += pitchInput * (g_flight.onGround ? 0.35f : 0.75f) * deltaTime;
    g_flight.roll += rollInput * (g_flight.onGround ? 0.25f : 0.95f) * deltaTime;
    g_flight.yaw += yawInput * 0.55f * deltaTime;
    g_flight.pitch = Clamp(g_flight.pitch, g_flight.onGround ? -0.10f : -0.35f,
                            g_flight.onGround ? 0.16f : 0.55f);
    g_flight.roll = Clamp(g_flight.roll, g_flight.onGround ? -0.12f : -0.85f,
                           g_flight.onGround ? 0.12f : 0.85f);
    if (rollInput == 0.0f) g_flight.roll *= 1.0f - 1.8f * deltaTime;

    forwardX = sinf(g_flight.yaw) * cosf(g_flight.pitch);
    forwardY = sinf(g_flight.pitch);
    forwardZ = -cosf(g_flight.yaw) * cosf(g_flight.pitch);

    forwardSpeed = g_velocityX * forwardX + g_velocityY * forwardY + g_velocityZ * forwardZ;
    airspeed = sqrtf(g_velocityX * g_velocityX + g_velocityY * g_velocityY + g_velocityZ * g_velocityZ);
    const float thrustFactor = Clamp((ENGINE_FULL_POWER_SPEED - forwardSpeed) / 20.0f, 0.0f, 1.0f);
    const float thrust = ENGINE_ACCELERATION * g_flight.throttle * thrustFactor;
    g_velocityX += forwardX * thrust * deltaTime;
    g_velocityY += forwardY * thrust * deltaTime;
    g_velocityZ += forwardZ * thrust * deltaTime;

    if (forwardSpeed > STALL_SPEED) {
        lift = (forwardSpeed - STALL_SPEED) * (forwardSpeed - STALL_SPEED) * 0.022f * cosf(g_flight.roll);
    }
    /* While rolling, lift can overcome gravity and starts the takeoff. */
    if (!g_flight.onGround || lift > 9.81f) g_velocityY += (lift - 9.81f) * deltaTime;

    /* Quadratic drag and reduced thrust yield a stable cruise speed instead
       of allowing acceleration to continue after takeoff. */
    airspeed = sqrtf(g_velocityX * g_velocityX + g_velocityY * g_velocityY + g_velocityZ * g_velocityZ);
    const float dragMultiplier = Clamp(1.0f - (PARASITIC_DRAG * airspeed + lift * 0.00035f) * deltaTime, 0.0f, 1.0f);
    g_velocityX *= dragMultiplier;
    g_velocityY *= dragMultiplier;
    g_velocityZ *= dragMultiplier;
    if (airspeed > MAX_AIRSPEED) {
        const float limiter = MAX_AIRSPEED / airspeed;
        g_velocityX *= limiter; g_velocityY *= limiter; g_velocityZ *= limiter;
    }
    g_flight.x += g_velocityX * deltaTime;
    g_flight.y += g_velocityY * deltaTime;
    g_flight.z += g_velocityZ * deltaTime;

    const float groundHeight = AircraftSurfaceHeight(g_flight.x, g_flight.z, g_flight.yaw)
                             + AircraftClearanceForAttitude(g_flight.pitch, g_flight.roll);
    if (g_flight.y <= groundHeight) {
        g_flight.y = groundHeight;
        if (g_velocityY < 0.0f) g_velocityY = 0.0f;
        g_flight.onGround = 1;
        const float groundFriction = Terrain_IsRunway(g_flight.x, g_flight.z) ? 0.12f : 1.20f;
        g_velocityX *= 1.0f - groundFriction * deltaTime;
        g_velocityZ *= 1.0f - groundFriction * deltaTime;
    } else {
        g_flight.onGround = 0;
    }

    g_flight.speed = sqrtf(g_velocityX * g_velocityX + g_velocityY * g_velocityY + g_velocityZ * g_velocityZ);
    g_flight.altitude = fmaxf(0.0f, g_flight.y - groundHeight);
}

const FlightState* Flight_GetState(void) {
    return &g_flight;
}
