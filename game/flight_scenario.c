#include <string.h>
#include <math.h>
#include "flight_scenario.h"
#include "flight.h"
#include "world/terrain.h"

#define PI_RADIANS 3.14159265f
#define FULL_CIRCLE_RADIANS (PI_RADIANS * 2.0f)
#define APPROACH_Z 260.0f

static float Clamp(float value, float minimum, float maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static float ReturnPitch(const FlightState* state, int banked) {
    const float basePitch = banked ? 0.14f : 0.02f;
    const float altitudeGain = banked ? 0.012f : 0.008f;
    return Clamp(basePitch + (70.0f - state->altitude) * altitudeGain, -0.12f, 0.25f);
}

static float TargetAxis(float current, float target) {
    if (current < target - 0.015f) return 1.0f;
    if (current > target + 0.015f) return -1.0f;
    return 0.0f;
}

void FlightScenario_Init(FlightScenario* scenario) {
    if (!scenario) return;
    memset(scenario, 0, sizeof(*scenario));
    scenario->phase = FLIGHT_SCENARIO_IDLE;
}

void FlightScenario_Start(FlightScenario* scenario) {
    if (!scenario) return;
    Flight_Reset();
    scenario->phase = FLIGHT_SCENARIO_RUN;
    scenario->phaseTime = 0.0f;
    scenario->failure = NULL;
}

void FlightScenario_Cancel(FlightScenario* scenario) {
    if (!scenario) return;
    scenario->phase = FLIGHT_SCENARIO_IDLE;
    scenario->phaseTime = 0.0f;
    scenario->failure = NULL;
}

int FlightScenario_IsActive(const FlightScenario* scenario) {
    return scenario && scenario->phase >= FLIGHT_SCENARIO_RUN && scenario->phase <= FLIGHT_SCENARIO_LAND;
}

void FlightScenario_BuildInput(const FlightScenario* scenario, const FlightState* state, FlightInput* input) {
    float targetThrottle = 0.0f;
    float targetPitch = 0.0f;
    if (!scenario || !state || !input) return;
    memset(input, 0, sizeof(*input));
    switch (scenario->phase) {
        case FLIGHT_SCENARIO_RUN:
            targetThrottle = 1.0f;
            if (state->speed * 3.6f > 135.0f) targetPitch = 0.13f;
            break;
        case FLIGHT_SCENARIO_CLIMB:
            targetThrottle = 1.0f;
            targetPitch = 0.20f;
            break;
        case FLIGHT_SCENARIO_CIRCLE:
            /* 46 degrees keeps the circle inside the small map while still
               being a proper visibly banked, coordinated turn. */
            targetThrottle = 0.65f;
            /* The vertical component of lift is reduced by the bank.  Hold
               125 m with a pitch target that rises as the aircraft sinks. */
            targetPitch = Clamp(0.08f + (135.0f - state->altitude) * 0.006f, -0.04f, 0.25f);
            input->roll = TargetAxis(state->roll, 0.80f);
            break;
        case FLIGHT_SCENARIO_RETURN_TURN_OUT:
            targetThrottle = 0.60f;
            targetPitch = ReturnPitch(state, 1);
            input->roll = TargetAxis(state->roll, 0.80f);
            break;
        case FLIGHT_SCENARIO_RETURN_TURN_IN:
            /* The inner half-turn starts slightly faster than the outbound
               one.  A tighter bank keeps its radius matched to the return
               leg and brings the aircraft back onto the runway centreline. */
            targetThrottle = 0.52f;
            targetPitch = ReturnPitch(state, 1);
            input->roll = TargetAxis(state->roll, 0.85f);
            break;
        case FLIGHT_SCENARIO_RETURN_LEG:
            targetThrottle = 0.60f;
            targetPitch = ReturnPitch(state, 0);
            break;
        case FLIGHT_SCENARIO_APPROACH:
            /* Give the banked turn a moment to unload before the glide. */
            targetThrottle = 0.52f;
            targetPitch = 0.02f;
            /* Counter the small leftward side velocity left by the final
               turn, then let the glide start on the runway centreline. */
            input->roll = TargetAxis(state->roll, 0.16f);
            break;
        case FLIGHT_SCENARIO_CRUISE:
            targetThrottle = 0.58f;
            targetPitch = 0.0f;
            break;
        case FLIGHT_SCENARIO_DESCENT:
            targetThrottle = 0.0f;
            targetPitch = -0.16f;
            break;
        case FLIGHT_SCENARIO_LAND:
            targetThrottle = 0.0f;
            targetPitch = 0.05f;
            break;
        default:
            return;
    }
    input->throttle = TargetAxis(state->throttle, targetThrottle);
    input->pitch = TargetAxis(state->pitch, targetPitch);
}

void FlightScenario_Observe(FlightScenario* scenario, const FlightState* state, float deltaTime) {
    if (!FlightScenario_IsActive(scenario) || !state) return;
    scenario->phaseTime += deltaTime;
    if (state->speed > 89.9f) { scenario->phase = FLIGHT_SCENARIO_FAIL; scenario->failure = "OVERSPEED"; return; }
    if (scenario->phaseTime > ((scenario->phase >= FLIGHT_SCENARIO_CIRCLE && scenario->phase <= FLIGHT_SCENARIO_RETURN_TURN_IN) ? 60.0f : 45.0f)) {
        scenario->phase = FLIGHT_SCENARIO_FAIL; scenario->failure = "TIMEOUT"; return;
    }
    switch (scenario->phase) {
        case FLIGHT_SCENARIO_RUN:
            if (!state->onGround && state->altitude > 4.0f) { scenario->phase = FLIGHT_SCENARIO_CLIMB; scenario->phaseTime = 0.0f; }
            else if (scenario->phaseTime > 25.0f) { scenario->phase = FLIGHT_SCENARIO_FAIL; scenario->failure = "NO TAKEOFF"; }
            break;
        case FLIGHT_SCENARIO_CLIMB:
            if (state->altitude >= 120.0f) {
                scenario->phase = FLIGHT_SCENARIO_CIRCLE;
                scenario->phaseTime = 0.0f;
                scenario->circleStartYaw = state->yaw;
            }
            else if (scenario->phaseTime > 25.0f) { scenario->phase = FLIGHT_SCENARIO_FAIL; scenario->failure = "NO CLIMB"; }
            break;
        case FLIGHT_SCENARIO_CIRCLE:
            if (state->yaw - scenario->circleStartYaw >= FULL_CIRCLE_RADIANS) {
                scenario->circleCompleted = 1;
                scenario->phase = FLIGHT_SCENARIO_RETURN_TURN_OUT;
                scenario->phaseTime = 0.0f;
            } else if (scenario->phaseTime > 52.0f) {
                scenario->phase = FLIGHT_SCENARIO_FAIL;
                scenario->failure = "NO CIRCLE";
            }
            break;
        case FLIGHT_SCENARIO_RETURN_TURN_OUT:
            if (state->yaw >= scenario->circleStartYaw + FULL_CIRCLE_RADIANS + PI_RADIANS) {
                scenario->phase = FLIGHT_SCENARIO_RETURN_LEG;
                scenario->phaseTime = 0.0f;
            } else if (scenario->phaseTime > 30.0f) {
                scenario->phase = FLIGHT_SCENARIO_FAIL;
                scenario->failure = "NO TURN OUT";
            }
            break;
        case FLIGHT_SCENARIO_RETURN_LEG:
            if (state->z >= APPROACH_Z) {
                scenario->phase = FLIGHT_SCENARIO_RETURN_TURN_IN;
                scenario->phaseTime = 0.0f;
            } else if (scenario->phaseTime > 32.0f) {
                scenario->phase = FLIGHT_SCENARIO_FAIL;
                scenario->failure = "NO RETURN LEG";
            }
            break;
        case FLIGHT_SCENARIO_RETURN_TURN_IN:
            if (state->yaw >= scenario->circleStartYaw + FULL_CIRCLE_RADIANS * 2.0f) {
                if (fabsf(state->x) <= 45.0f && state->z >= 210.0f && state->z <= 450.0f) {
                    scenario->phase = FLIGHT_SCENARIO_APPROACH;
                    scenario->phaseTime = 0.0f;
                } else {
                    scenario->phase = FLIGHT_SCENARIO_FAIL;
                    scenario->failure = "NO APPROACH";
                }
            } else if (scenario->phaseTime > 30.0f) {
                scenario->phase = FLIGHT_SCENARIO_FAIL;
                scenario->failure = "NO TURN IN";
            }
            break;
        case FLIGHT_SCENARIO_APPROACH:
            if (scenario->phaseTime >= 3.0f) {
                scenario->phase = FLIGHT_SCENARIO_DESCENT;
                scenario->phaseTime = 0.0f;
            }
            break;
        case FLIGHT_SCENARIO_CRUISE:
            if (scenario->phaseTime >= 4.0f) { scenario->phase = FLIGHT_SCENARIO_DESCENT; scenario->phaseTime = 0.0f; }
            break;
        case FLIGHT_SCENARIO_DESCENT:
            if (state->altitude < 8.0f) { scenario->phase = FLIGHT_SCENARIO_LAND; scenario->phaseTime = 0.0f; }
            else if (scenario->phaseTime > 35.0f) { scenario->phase = FLIGHT_SCENARIO_FAIL; scenario->failure = "NO DESCENT"; }
            break;
        case FLIGHT_SCENARIO_LAND:
            if (state->onGround && state->speed < 25.0f && Terrain_IsRunway(state->x, state->z)) {
                scenario->phase = FLIGHT_SCENARIO_PASS; scenario->phaseTime = 0.0f;
            } else if (state->onGround && !Terrain_IsRunway(state->x, state->z)) {
                scenario->phase = FLIGHT_SCENARIO_FAIL; scenario->failure = "OFF RUNWAY";
            }
            else if (scenario->phaseTime > 20.0f) { scenario->phase = FLIGHT_SCENARIO_FAIL; scenario->failure = "NO LAND"; }
            break;
        default: break;
    }
}

const char* FlightScenario_Status(const FlightScenario* scenario) {
    if (!scenario) return "TST OFF";
    switch (scenario->phase) {
        case FLIGHT_SCENARIO_RUN: return "TST RUN";
        case FLIGHT_SCENARIO_CLIMB: return "TST CLIMB";
        case FLIGHT_SCENARIO_CIRCLE: return "TST CIRCLE";
        case FLIGHT_SCENARIO_RETURN_TURN_OUT: return "TST TURN OUT";
        case FLIGHT_SCENARIO_RETURN_LEG: return "TST RETURN";
        case FLIGHT_SCENARIO_RETURN_TURN_IN: return "TST TURN IN";
        case FLIGHT_SCENARIO_APPROACH: return "TST APPROACH";
        case FLIGHT_SCENARIO_CRUISE: return "TST CRUISE";
        case FLIGHT_SCENARIO_DESCENT: return "TST DESCENT";
        case FLIGHT_SCENARIO_LAND: return "TST LAND";
        case FLIGHT_SCENARIO_PASS: return "TST PASS";
        case FLIGHT_SCENARIO_FAIL: return scenario->failure ? scenario->failure : "TST FAIL";
        default: return "TST OFF";
    }
}
