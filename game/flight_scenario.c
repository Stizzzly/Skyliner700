#include <string.h>
#include "flight_scenario.h"
#include "flight.h"

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
    if (scenario->phaseTime > 45.0f) { scenario->phase = FLIGHT_SCENARIO_FAIL; scenario->failure = "TIMEOUT"; return; }
    switch (scenario->phase) {
        case FLIGHT_SCENARIO_RUN:
            if (!state->onGround && state->altitude > 4.0f) { scenario->phase = FLIGHT_SCENARIO_CLIMB; scenario->phaseTime = 0.0f; }
            else if (scenario->phaseTime > 25.0f) { scenario->phase = FLIGHT_SCENARIO_FAIL; scenario->failure = "NO TAKEOFF"; }
            break;
        case FLIGHT_SCENARIO_CLIMB:
            if (state->altitude >= 100.0f) { scenario->phase = FLIGHT_SCENARIO_CRUISE; scenario->phaseTime = 0.0f; }
            else if (scenario->phaseTime > 25.0f) { scenario->phase = FLIGHT_SCENARIO_FAIL; scenario->failure = "NO CLIMB"; }
            break;
        case FLIGHT_SCENARIO_CRUISE:
            if (scenario->phaseTime >= 4.0f) { scenario->phase = FLIGHT_SCENARIO_DESCENT; scenario->phaseTime = 0.0f; }
            break;
        case FLIGHT_SCENARIO_DESCENT:
            if (state->altitude < 8.0f) { scenario->phase = FLIGHT_SCENARIO_LAND; scenario->phaseTime = 0.0f; }
            else if (scenario->phaseTime > 35.0f) { scenario->phase = FLIGHT_SCENARIO_FAIL; scenario->failure = "NO DESCENT"; }
            break;
        case FLIGHT_SCENARIO_LAND:
            if (state->onGround && state->speed < 25.0f) { scenario->phase = FLIGHT_SCENARIO_PASS; scenario->phaseTime = 0.0f; }
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
        case FLIGHT_SCENARIO_CRUISE: return "TST CRUISE";
        case FLIGHT_SCENARIO_DESCENT: return "TST DESCENT";
        case FLIGHT_SCENARIO_LAND: return "TST LAND";
        case FLIGHT_SCENARIO_PASS: return "TST PASS";
        case FLIGHT_SCENARIO_FAIL: return scenario->failure ? scenario->failure : "TST FAIL";
        default: return "TST OFF";
    }
}
