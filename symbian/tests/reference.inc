// Original floating-point model, desktop differential tests ONLY.
namespace Reference {

/* Controls are normalised to [-1, 1], which makes the simulation independent
 * of Windows keyboard state and deterministic in command-line tests. */
typedef struct {
    float pitch;
    float roll;
    float yaw;
    float throttle;
    int reset;
} FlightInput;

typedef struct {
    float x, y, z;
    float pitch, yaw, roll;
    float speed;
    float throttle;
    float altitude;
    float verticalSpeed;
    float lift;
    float drag;
    /* Angles in radians.  They are kept in the state so deterministic tests
       and the dev HUD can inspect the actual aerodynamic input. */
    float angleOfAttack;
    float flightPathAngle;
    int onGround;
} FlightState;

typedef struct {
    FlightState state;
    float velocityX, velocityY, velocityZ;
} FlightModel;

void FlightModel_Reset(FlightModel* model);
void FlightModel_Step(FlightModel* model, const FlightInput* input, float deltaTime);
const FlightState* FlightModel_GetState(const FlightModel* model);

/* The game owns one player model; tests can create additional FlightModel
 * values and call the pure API above directly. */
void Flight_Init(void);
void Flight_Reset(void);
void Flight_Step(const FlightInput* input, float deltaTime);
const FlightState* Flight_GetState(void);


/* The renderer and flight model share one deterministic terrain equation,
 * so visual ground and landing collision cannot drift apart. */
float Terrain_GetHeight(float x, float z);
float Terrain_GetSurfaceHeight(float x, float z);
int Terrain_IsRunway(float x, float z);


static float TerrainClamp(float value, float minimum, float maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

float Terrain_GetHeight(float x, float z) {
    const float hills = sinf(x * 0.012f) * cosf(z * 0.011f) * 17.0f + sinf((x + z) * 0.0065f) * 8.0f;
    const float airportDistance = fmaxf(fabsf(x) - 100.0f, fabsf(z) - 360.0f);
    return hills * TerrainClamp(airportDistance / 90.0f, 0.0f, 1.0f);
}

int Terrain_IsRunway(float x, float z) { return fabsf(x) <= 15.0f && fabsf(z) <= 300.0f; }

float Terrain_GetSurfaceHeight(float x, float z) {
    if (Terrain_IsRunway(x, z)) return 0.055f;
    if (x >= 24.0f && x <= 95.0f && z >= -65.0f && z <= 30.0f) return 0.035f;
    return Terrain_GetHeight(x, z);
}


#define AIRCRAFT_CLEARANCE 0.53f
#define STALL_SPEED 18.0f
#define ENGINE_ACCELERATION 18.0f
#define ENGINE_FULL_POWER_SPEED 70.0f
#define MAX_AIRSPEED 90.0f
#define PARASITIC_DRAG 0.0026f
#define GRAVITY 9.81f
#define CRUISE_SPEED 50.0f
#define LIFT_COEFFICIENT_ZERO_AOA 1.0f
#define LIFT_SLOPE 4.0f
#define MIN_AOA -0.30f
#define MAX_AOA 0.45f
#define MIN_TURN_SPEED 20.0f

#ifndef SKYLINER_NO_GLOBAL_API
static FlightModel g_playerFlight;
#endif

static float Clamp(float value, float minimum, float maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static float AircraftSurfaceHeight(float x, float z, float yaw) {
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
    return AIRCRAFT_CLEARANCE + fabsf(sinf(pitch)) * 2.8f + fabsf(sinf(roll)) * 3.6f;
}

void FlightModel_Reset(FlightModel* model) {
    if (!model) return;
    memset(model, 0, sizeof(*model));
    model->state.x = 0.0f;
    model->state.z = 220.0f;
    model->state.y = AircraftSurfaceHeight(model->state.x, model->state.z, 0.0f) + AIRCRAFT_CLEARANCE;
    model->state.onGround = 1;
}

void FlightModel_Step(FlightModel* model, const FlightInput* input, float deltaTime) {
    FlightState* state;
    float forwardX, forwardY, forwardZ;
    float forwardSpeed, airspeed, lift = 0.0f;
    float horizontalSpeed, flightPathAngle, angleOfAttack, liftCoefficient;
    float groundHeight;
    FlightInput noInput = {0};
    if (!model) return;
    if (!input) input = &noInput;
    if (input->reset) { FlightModel_Reset(model); return; }
    if (deltaTime <= 0.0f) return;
    if (deltaTime > 0.05f) deltaTime = 0.05f;
    state = &model->state;

    state->throttle = Clamp(state->throttle + Clamp(input->throttle, -1.0f, 1.0f) * 0.35f * deltaTime, 0.0f, 1.0f);
    state->pitch += Clamp(input->pitch, -1.0f, 1.0f) * (state->onGround ? 0.35f : 0.75f) * deltaTime;
    state->roll += Clamp(input->roll, -1.0f, 1.0f) * (state->onGround ? 0.25f : 0.95f) * deltaTime;
    state->yaw += Clamp(input->yaw, -1.0f, 1.0f) * 0.55f * deltaTime;
    state->pitch = Clamp(state->pitch, state->onGround ? -0.10f : -0.35f, state->onGround ? 0.16f : 0.55f);
    state->roll = Clamp(state->roll, state->onGround ? -0.12f : -0.85f, state->onGround ? 0.12f : 0.85f);
    if (input->roll == 0.0f) state->roll *= Clamp(1.0f - 1.8f * deltaTime, 0.0f, 1.0f);

    forwardX = sinf(state->yaw) * cosf(state->pitch);
    forwardY = sinf(state->pitch);
    forwardZ = -cosf(state->yaw) * cosf(state->pitch);
    forwardSpeed = model->velocityX * forwardX + model->velocityY * forwardY + model->velocityZ * forwardZ;
    airspeed = sqrtf(model->velocityX * model->velocityX + model->velocityY * model->velocityY + model->velocityZ * model->velocityZ);

    /* A bank tilts the lift vector.  Its horizontal component curves the
       velocity and the body follows that coordinated turn.  This is what
       makes Left/Right useful in flight instead of being a visual-only roll. */
    horizontalSpeed = sqrtf(model->velocityX * model->velocityX + model->velocityZ * model->velocityZ);
    if (!state->onGround && horizontalSpeed > MIN_TURN_SPEED) {
        /* 0.1.10 intentional bug fix, shared with the fixed-point model:
           right-wing-down roll must increase yaw toward +X. */
        const float bankTurnRate = GRAVITY * tanf(state->roll) / horizontalSpeed;
        state->yaw += bankTurnRate * deltaTime;
        forwardX = sinf(state->yaw) * cosf(state->pitch);
        forwardY = sinf(state->pitch);
        forwardZ = -cosf(state->yaw) * cosf(state->pitch);
        forwardSpeed = model->velocityX * forwardX + model->velocityY * forwardY + model->velocityZ * forwardZ;
    }
    const float thrustFactor = Clamp((ENGINE_FULL_POWER_SPEED - forwardSpeed) / 20.0f, 0.0f, 1.0f);
    const float thrust = ENGINE_ACCELERATION * state->throttle * thrustFactor;
    model->velocityX += forwardX * thrust * deltaTime;
    model->velocityY += forwardY * thrust * deltaTime;
    model->velocityZ += forwardZ * thrust * deltaTime;

    /* The wing does not react to the model's pitch alone.  It reacts to the
       angle between the nose and the velocity vector (angle of attack).  This
       gives the controls their expected meaning: at the same airspeed, nose
       up establishes a climbing flight path and nose down establishes a
       descending one. */
    horizontalSpeed = sqrtf(model->velocityX * model->velocityX + model->velocityZ * model->velocityZ);
    flightPathAngle = atan2f(model->velocityY, fmaxf(horizontalSpeed, 0.1f));
    angleOfAttack = Clamp(state->pitch - flightPathAngle, MIN_AOA, MAX_AOA);
    liftCoefficient = LIFT_COEFFICIENT_ZERO_AOA + LIFT_SLOPE * angleOfAttack;
    if (angleOfAttack > 0.30f) {
        /* A deliberately gentle post-stall falloff.  The game remains
           controllable, but pulling hard cannot create unlimited lift. */
        liftCoefficient = 2.20f - (angleOfAttack - 0.30f) * 2.5f;
    }
    liftCoefficient = Clamp(liftCoefficient, -0.35f, 2.20f);
    if (forwardSpeed > STALL_SPEED && liftCoefficient > 0.0f) {
        const float speedRatio = forwardSpeed / CRUISE_SPEED;
        lift = GRAVITY * speedRatio * speedRatio * liftCoefficient * cosf(state->roll);
    }
    if (!state->onGround || lift > GRAVITY) {
        const float verticalLift = lift * cosf(state->roll);
        const float lateralLift = lift * sinf(state->roll);
        const float rightX = cosf(state->yaw);
        const float rightZ = sinf(state->yaw);
        model->velocityY += (verticalLift - GRAVITY) * deltaTime;
        model->velocityX += rightX * lateralLift * deltaTime;
        model->velocityZ += rightZ * lateralLift * deltaTime;
    }

    airspeed = sqrtf(model->velocityX * model->velocityX + model->velocityY * model->velocityY + model->velocityZ * model->velocityZ);
    const float dragRate = PARASITIC_DRAG * airspeed + lift * 0.00035f;
    const float dragMultiplier = Clamp(1.0f - dragRate * deltaTime, 0.0f, 1.0f);
    model->velocityX *= dragMultiplier;
    model->velocityY *= dragMultiplier;
    model->velocityZ *= dragMultiplier;
    airspeed = sqrtf(model->velocityX * model->velocityX + model->velocityY * model->velocityY + model->velocityZ * model->velocityZ);
    if (airspeed > MAX_AIRSPEED) {
        const float limiter = MAX_AIRSPEED / airspeed;
        model->velocityX *= limiter; model->velocityY *= limiter; model->velocityZ *= limiter;
    }

    state->x += model->velocityX * deltaTime;
    state->y += model->velocityY * deltaTime;
    state->z += model->velocityZ * deltaTime;
    groundHeight = AircraftSurfaceHeight(state->x, state->z, state->yaw) + AircraftClearanceForAttitude(state->pitch, state->roll);
    if (state->y <= groundHeight + 0.01f) {
        state->y = groundHeight;
        if (model->velocityY < 0.0f) model->velocityY = 0.0f;
        state->onGround = 1;
        const float groundFriction = Terrain_IsRunway(state->x, state->z) ? 0.12f : 1.20f;
        const float friction = Clamp(1.0f - groundFriction * deltaTime, 0.0f, 1.0f);
        model->velocityX *= friction;
        model->velocityZ *= friction;
    } else {
        state->onGround = 0;
    }
    state->speed = sqrtf(model->velocityX * model->velocityX + model->velocityY * model->velocityY + model->velocityZ * model->velocityZ);
    state->verticalSpeed = model->velocityY;
    state->lift = lift;
    state->drag = dragRate * state->speed;
    state->angleOfAttack = angleOfAttack;
    state->flightPathAngle = flightPathAngle;
    state->altitude = fmaxf(0.0f, state->y - groundHeight);
}

const FlightState* FlightModel_GetState(const FlightModel* model) { return model ? &model->state : NULL; }
#ifndef SKYLINER_NO_GLOBAL_API
void Flight_Init(void) { FlightModel_Reset(&g_playerFlight); }
void Flight_Reset(void) { FlightModel_Reset(&g_playerFlight); }
void Flight_Step(const FlightInput* input, float deltaTime) { FlightModel_Step(&g_playerFlight, input, deltaTime); }
const FlightState* Flight_GetState(void) { return &g_playerFlight.state; }
#endif

}
