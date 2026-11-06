#ifdef __SYMBIAN32__
#define SKYLINER_NO_GLOBAL_API
#endif
#include "fixed.h"
#include "reciprocal.h"
#include <string.h>
#include "game/flight.h"
#include "world/terrain.h"

#define AIRCRAFT_CLEARANCE Fix::Raw(34734)
#define STALL_SPEED Fix::Raw(1179648)
#define ENGINE_ACCELERATION Fix::Raw(1179648)
#define ENGINE_FULL_POWER_SPEED Fix::Raw(4587520)
#define MAX_AIRSPEED Fix::Raw(5898240)
#define PARASITIC_DRAG Fix::Raw(170)
#define GRAVITY Fix::Raw(642908)
#define CRUISE_SPEED Fix::Raw(3276800)
#define LIFT_COEFFICIENT_ZERO_AOA Fix::Raw(65536)
#define LIFT_SLOPE Fix::Raw(262144)
#define MIN_AOA -Fix::Raw(19661)
#define MAX_AOA Fix::Raw(29491)
#define MIN_TURN_SPEED Fix::Raw(1310720)

#ifndef SKYLINER_NO_GLOBAL_API
static FlightModel g_playerFlight;
#endif

static Fix Clamp(Fix value, Fix minimum, Fix maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static Fix AircraftSurfaceHeight(Fix x, Fix z, Fix yaw) {
    static const int contacts[][2] = {{0,-209715},{0,206438},{-258867,36045},{258867,36045},{0,0}};
    Fix highest = -Fix(30000);
    const Fix cosine = FxCos(yaw);
    const Fix sine = FxSin(yaw);
    for (int point = 0; point < (int)(sizeof(contacts) / sizeof(contacts[0])); ++point) {
        const Fix localX = Fix::Raw(contacts[point][0]);
        const Fix localZ = Fix::Raw(contacts[point][1]);
        const Fix worldX = x + localX * cosine + localZ * sine;
        const Fix worldZ = z - localX * sine + localZ * cosine;
        const Fix height = Terrain_GetSurfaceHeight(worldX, worldZ);
        if (height > highest) highest = height;
    }
    return highest;
}

static Fix AircraftClearanceForAttitude(Fix pitch, Fix roll) {
    return AIRCRAFT_CLEARANCE + FxAbs(FxSin(pitch)) * Fix::Raw(183501) + FxAbs(FxSin(roll)) * Fix::Raw(235930);
}

void FlightModel_Reset(FlightModel* model) {
    if (!model) return;
    memset(model, 0, sizeof(*model));
    model->state.x = Fix::Raw(0);
    model->state.z = Fix::Raw(14417920);
    model->state.y = AircraftSurfaceHeight(model->state.x, model->state.z, Fix::Raw(0)) + AIRCRAFT_CLEARANCE;
    model->state.onGround = 1;
}

void FlightModel_Step(FlightModel* model, const FlightInput* input, Fix deltaTime) {
    FlightState* state;
    Fix forwardX, forwardY, forwardZ;
    Fix forwardSpeed, airspeed, lift = Fix::Raw(0);
    Fix horizontalSpeed, flightPathAngle, angleOfAttack, liftCoefficient;
    Fix groundHeight;
    FlightInput noInput = {0};
    if (!model) return;
    if (!input) input = &noInput;
    if (input->reset) { FlightModel_Reset(model); return; }
    if (deltaTime <= Fix::Raw(0)) return;
    if (deltaTime > Fix::Raw(3277)) deltaTime = Fix::Raw(3277);
    state = &model->state;

    state->throttle = Clamp(state->throttle + Clamp(input->throttle, -Fix::Raw(65536), Fix::Raw(65536)) * Fix::Raw(22938) * deltaTime, Fix::Raw(0), Fix::Raw(65536));
    state->pitch += Clamp(input->pitch, -Fix::Raw(65536), Fix::Raw(65536)) * (state->onGround ? Fix::Raw(22938) : Fix::Raw(49152)) * deltaTime;
    state->roll += Clamp(input->roll, -Fix::Raw(65536), Fix::Raw(65536)) * (state->onGround ? Fix::Raw(16384) : Fix::Raw(62259)) * deltaTime;
    state->yaw += Clamp(input->yaw, -Fix::Raw(65536), Fix::Raw(65536)) * Fix::Raw(36045) * deltaTime;
    state->pitch = Clamp(state->pitch, state->onGround ? -Fix::Raw(6554) : -Fix::Raw(22938), state->onGround ? Fix::Raw(10486) : Fix::Raw(36045));
    state->roll = Clamp(state->roll, state->onGround ? -Fix::Raw(7864) : -Fix::Raw(55706), state->onGround ? Fix::Raw(7864) : Fix::Raw(55706));
    if (input->roll == Fix::Raw(0)) state->roll *= Clamp(Fix::Raw(65536) - Fix::Raw(117965) * deltaTime, Fix::Raw(0), Fix::Raw(65536));

    forwardX = FxSin(state->yaw) * FxCos(state->pitch);
    forwardY = FxSin(state->pitch);
    forwardZ = -FxCos(state->yaw) * FxCos(state->pitch);
    forwardSpeed = model->velocityX * forwardX + model->velocityY * forwardY + model->velocityZ * forwardZ;
    airspeed = FxSqrt(model->velocityX * model->velocityX + model->velocityY * model->velocityY + model->velocityZ * model->velocityZ);

    /* A bank tilts the lift vector.  Its horizontal component curves the
       velocity and the body follows that coordinated turn.  This is what
       makes Left/Right useful in flight instead of being a visual-only roll. */
    horizontalSpeed = FxSqrt(model->velocityX * model->velocityX + model->velocityZ * model->velocityZ);
    if (!state->onGround && horizontalSpeed > MIN_TURN_SPEED) {
        /* Positive roll lowers the right wing. With forwardX=sin(yaw),
           positive yaw turns toward +X, so bank and yaw have the same sign. */
        const Fix bankTurnRate = Fix::Raw(FixedSpeedDivide((GRAVITY * FxTan(state->roll)).raw,(unsigned)horizontalSpeed.raw,SpeedInverse((unsigned)horizontalSpeed.raw)));
        state->yaw += bankTurnRate * deltaTime;
        forwardX = FxSin(state->yaw) * FxCos(state->pitch);
        forwardY = FxSin(state->pitch);
        forwardZ = -FxCos(state->yaw) * FxCos(state->pitch);
        forwardSpeed = model->velocityX * forwardX + model->velocityY * forwardY + model->velocityZ * forwardZ;
    }
    const Fix thrustFactor = Clamp(Fix::Raw(FixedSpeedDivide((ENGINE_FULL_POWER_SPEED-forwardSpeed).raw,1310720u,214748364u)), Fix::Raw(0), Fix::Raw(65536));
    const Fix thrust = ENGINE_ACCELERATION * state->throttle * thrustFactor;
    model->velocityX += forwardX * thrust * deltaTime;
    model->velocityY += forwardY * thrust * deltaTime;
    model->velocityZ += forwardZ * thrust * deltaTime;

    /* The wing does not react to the model's pitch alone.  It reacts to the
       angle between the nose and the velocity vector (angle of attack).  This
       gives the controls their expected meaning: at the same airspeed, nose
       up establishes a climbing flight path and nose down establishes a
       descending one. */
    horizontalSpeed = FxSqrt(model->velocityX * model->velocityX + model->velocityZ * model->velocityZ);
    flightPathAngle = FxAtan2(model->velocityY, FxMax(horizontalSpeed, Fix::Raw(6554)));
    angleOfAttack = Clamp(state->pitch - flightPathAngle, MIN_AOA, MAX_AOA);
    liftCoefficient = LIFT_COEFFICIENT_ZERO_AOA + LIFT_SLOPE * angleOfAttack;
    if (angleOfAttack > Fix::Raw(19661)) {
        /* A deliberately gentle post-stall falloff.  The game remains
           controllable, but pulling hard cannot create unlimited lift. */
        liftCoefficient = Fix::Raw(144179) - (angleOfAttack - Fix::Raw(19661)) * Fix::Raw(163840);
    }
    liftCoefficient = Clamp(liftCoefficient, -Fix::Raw(22938), Fix::Raw(144179));
    if (forwardSpeed > STALL_SPEED && liftCoefficient > Fix::Raw(0)) {
        const Fix speedRatio = Fix::Raw(FixedSpeedDivide(forwardSpeed.raw,3276800u,85899345u));
        lift = GRAVITY * speedRatio * speedRatio * liftCoefficient * FxCos(state->roll);
    }
    if (!state->onGround || lift > GRAVITY) {
        const Fix verticalLift = lift * FxCos(state->roll);
        const Fix lateralLift = lift * FxSin(state->roll);
        const Fix rightX = FxCos(state->yaw);
        const Fix rightZ = FxSin(state->yaw);
        model->velocityY += (verticalLift - GRAVITY) * deltaTime;
        model->velocityX += rightX * lateralLift * deltaTime;
        model->velocityZ += rightZ * lateralLift * deltaTime;
    }

    airspeed = FxSqrt(model->velocityX * model->velocityX + model->velocityY * model->velocityY + model->velocityZ * model->velocityZ);
    const Fix dragRate = PARASITIC_DRAG * airspeed + lift * Fix::Raw(23);
    const Fix dragMultiplier = Clamp(Fix::Raw(65536) - dragRate * deltaTime, Fix::Raw(0), Fix::Raw(65536));
    model->velocityX *= dragMultiplier;
    model->velocityY *= dragMultiplier;
    model->velocityZ *= dragMultiplier;
    airspeed = FxSqrt(model->velocityX * model->velocityX + model->velocityY * model->velocityY + model->velocityZ * model->velocityZ);
    if (airspeed > MAX_AIRSPEED) {
        const Fix limiter = Fix::Raw(FixedSpeedDivide(MAX_AIRSPEED.raw,(unsigned)airspeed.raw,SpeedInverse((unsigned)airspeed.raw)));
        model->velocityX *= limiter; model->velocityY *= limiter; model->velocityZ *= limiter;
    }

    state->yaw = FxWrapAngle(state->yaw);
    state->x += model->velocityX * deltaTime;
    state->y += model->velocityY * deltaTime;
    state->z += model->velocityZ * deltaTime;
    groundHeight = AircraftSurfaceHeight(state->x, state->z, state->yaw) + AircraftClearanceForAttitude(state->pitch, state->roll);
    if (state->y <= groundHeight + Fix::Raw(655)) {
        state->y = groundHeight;
        if (model->velocityY < Fix::Raw(0)) model->velocityY = Fix::Raw(0);
        state->onGround = 1;
        const Fix groundFriction = Terrain_IsRunway(state->x, state->z) ? Fix::Raw(7864) : Fix::Raw(78643);
        const Fix friction = Clamp(Fix::Raw(65536) - groundFriction * deltaTime, Fix::Raw(0), Fix::Raw(65536));
        model->velocityX *= friction;
        model->velocityZ *= friction;
    } else {
        state->onGround = 0;
    }
    state->speed = FxSqrt(model->velocityX * model->velocityX + model->velocityY * model->velocityY + model->velocityZ * model->velocityZ);
    state->verticalSpeed = model->velocityY;
    state->lift = lift;
    state->drag = dragRate * state->speed;
    state->angleOfAttack = angleOfAttack;
    state->flightPathAngle = flightPathAngle;
    state->altitude = FxMax(Fix::Raw(0), state->y - groundHeight);
}

const FlightState* FlightModel_GetState(const FlightModel* model) { return model ? &model->state : NULL; }
#ifndef SKYLINER_NO_GLOBAL_API
void Flight_Init(void) { FlightModel_Reset(&g_playerFlight); }
void Flight_Reset(void) { FlightModel_Reset(&g_playerFlight); }
void Flight_Step(const FlightInput* input, Fix deltaTime) { FlightModel_Step(&g_playerFlight, input, deltaTime); }
const FlightState* Flight_GetState(void) { return &g_playerFlight.state; }
#endif
