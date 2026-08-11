#include <math.h>
#include <stdio.h>
#include "game/flight.h"
#include "game/flight_scenario.h"

#define DT (1.0f / 120.0f)

static int g_failures;
#define CHECK(condition, message) do { if (!(condition)) { fprintf(stderr, "FAIL: %s\n", message); ++g_failures; } } while (0)

static void StepFor(FlightModel* model, FlightInput input, float seconds) {
    const int steps = (int)(seconds / DT);
    for (int step = 0; step < steps; ++step) FlightModel_Step(model, &input, DT);
}

int main(void) {
    FlightModel model;
    FlightInput input = {0};
    const FlightState* state;

    FlightModel_Reset(&model);
    state = FlightModel_GetState(&model);
    CHECK(state->onGround, "reset must start on ground");
    CHECK(state->altitude < 0.01f, "reset must not penetrate ground");

    input.pitch = 1.0f;
    StepFor(&model, input, 1.0f);
    state = FlightModel_GetState(&model);
    CHECK(state->pitch > 0.13f && state->pitch <= 0.161f, "ground rotation must reach about 9 degrees");
    CHECK(state->altitude < 0.01f, "ground rotation must preserve contact");

    FlightModel_Reset(&model);
    input = (FlightInput){ .throttle = 1.0f };
    StepFor(&model, input, 30.0f);
    state = FlightModel_GetState(&model);
    CHECK(!state->onGround, "full-throttle run must take off");
    CHECK(state->speed * 3.6f >= 140.0f && state->speed * 3.6f <= 324.1f, "takeoff speed must remain bounded");

    input.pitch = 0.30f;
    StepFor(&model, input, 3.0f);
    state = FlightModel_GetState(&model);
    CHECK(state->pitch > 0.20f, "airborne pitch input must have full authority");
    CHECK(state->altitude > 10.0f, "positive pitch must gain altitude");

    input = (FlightInput){ .roll = 1.0f };
    StepFor(&model, input, 1.0f);
    state = FlightModel_GetState(&model);
    CHECK(state->roll > 0.60f, "airborne roll input must bank the aircraft");

    const float yawBeforeTurn = state->yaw;
    input = (FlightInput){ .yaw = 1.0f };
    StepFor(&model, input, 1.0f);
    state = FlightModel_GetState(&model);
    CHECK(state->yaw > yawBeforeTurn + 0.50f, "yaw input must rotate heading");

    input = (FlightInput){ .throttle = -1.0f, .pitch = -0.40f };
    const float speedBeforeIdle = state->speed;
    StepFor(&model, input, 60.0f);
    state = FlightModel_GetState(&model);
    CHECK(state->speed < speedBeforeIdle, "idle thrust must slow the aircraft");
    CHECK(state->speed <= 90.01f, "absolute airspeed limit must hold");
    CHECK(state->onGround, "idle descent must return to terrain");

    FlightScenario scenario;
    FlightScenario_Init(&scenario);
    Flight_Init();
    FlightScenario_Start(&scenario);
    for (int step = 0; step < (int)(100.0f / DT) && FlightScenario_IsActive(&scenario); ++step) {
        FlightInput scenarioInput;
        FlightScenario_BuildInput(&scenario, Flight_GetState(), &scenarioInput);
        Flight_Step(&scenarioInput, DT);
        FlightScenario_Observe(&scenario, Flight_GetState(), DT);
    }
    CHECK(scenario.phase == FLIGHT_SCENARIO_PASS, "scripted takeoff and landing scenario must pass");

    if (g_failures) return 1;
    printf("flight physics tests passed\n");
    return 0;
}
