#include <math.h>
#include <stdio.h>
#include "game/flight.h"
#include "game/flight_scenario.h"
#include "world/terrain.h"

#define DT (1.0f / 120.0f)

static int g_failures;
#define CHECK(condition, message) do { if (!(condition)) { fprintf(stderr, "FAIL: %s\n", message); ++g_failures; } } while (0)

static void StepFor(FlightModel* model, FlightInput input, float seconds) {
    const int steps = (int)(seconds / DT);
    for (int step = 0; step < steps; ++step) FlightModel_Step(model, &input, DT);
}

static void SetAirborneAtSpeed(FlightModel* model, float speed, float pitch) {
    FlightModel_Reset(model);
    model->state.y += 300.0f;
    model->state.pitch = pitch;
    model->state.onGround = 0;
    model->velocityX = 0.0f;
    model->velocityY = 0.0f;
    model->velocityZ = -speed;
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

    /* The essential control-law checks: the same fast airflow must produce
       opposite vertical results for opposite pitch commands.  This prevents
       the old speed-only lift model from returning unnoticed. */
    SetAirborneAtSpeed(&model, 60.0f, 0.16f);
    const float climbStart = model.state.y;
    StepFor(&model, (FlightInput){0}, 1.5f);
    state = FlightModel_GetState(&model);
    CHECK(state->y > climbStart + 2.0f, "positive pitch at high speed must establish a climb");
    CHECK(state->verticalSpeed > 0.5f, "positive pitch at high speed must have positive vertical speed");

    SetAirborneAtSpeed(&model, 60.0f, -0.16f);
    const float descentStart = model.state.y;
    StepFor(&model, (FlightInput){0}, 1.5f);
    state = FlightModel_GetState(&model);
    CHECK(state->y < descentStart - 2.0f, "negative pitch at high speed must establish a descent");
    CHECK(state->verticalSpeed < -0.5f, "negative pitch at high speed must have negative vertical speed");
    CHECK(state->angleOfAttack > -0.30f && state->angleOfAttack < 0.30f,
          "angle of attack must settle independently of body pitch");

    SetAirborneAtSpeed(&model, 50.0f, 0.0f);
    const float levelStart = model.state.y;
    StepFor(&model, (FlightInput){0}, 2.0f);
    state = FlightModel_GetState(&model);
    CHECK(fabsf(state->y - levelStart) < 4.0f, "cruise speed with neutral pitch must remain near level");

    SetAirborneAtSpeed(&model, 55.0f, 0.03f);
    input = (FlightInput){ .roll = 1.0f };
    StepFor(&model, input, 2.0f);
    state = FlightModel_GetState(&model);
    CHECK(state->roll > 0.70f, "airborne roll input must bank the aircraft");
    CHECK(state->yaw > 0.30f, "positive bank must turn the aircraft without rudder input");

    SetAirborneAtSpeed(&model, 55.0f, 0.03f);
    input = (FlightInput){ .roll = -1.0f };
    StepFor(&model, input, 2.0f);
    state = FlightModel_GetState(&model);
    CHECK(state->yaw < -0.30f, "negative bank must turn the aircraft in the opposite direction");

    SetAirborneAtSpeed(&model, 55.0f, 0.03f);
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
    int sawCircle = 0;
    for (int step = 0; step < (int)(180.0f / DT) && FlightScenario_IsActive(&scenario); ++step) {
        FlightInput scenarioInput;
        FlightScenario_BuildInput(&scenario, Flight_GetState(), &scenarioInput);
        Flight_Step(&scenarioInput, DT);
        FlightScenario_Observe(&scenario, Flight_GetState(), DT);
        if (scenario.phase == FLIGHT_SCENARIO_CIRCLE) sawCircle = 1;
    }
    CHECK(sawCircle, "scripted scenario must enter the banked circle");
    CHECK(scenario.circleCompleted, "scripted scenario must complete a full circle");
    if (scenario.phase != FLIGHT_SCENARIO_PASS) {
        state = Flight_GetState();
        fprintf(stderr, "Scenario end: phase=%d failure=%s x=%.1f z=%.1f alt=%.1f speed=%.1f yaw=%.2f\n",
                scenario.phase, scenario.failure ? scenario.failure : "none",
                state->x, state->z, state->altitude, state->speed, state->yaw);
    }
    CHECK(scenario.phase == FLIGHT_SCENARIO_PASS, "scripted takeoff and landing scenario must pass");
    state = Flight_GetState();
    CHECK(state->onGround && Terrain_IsRunway(state->x, state->z),
          "scripted scenario must stop on the runway");

    if (g_failures) return 1;
    printf("flight physics tests passed\n");
    return 0;
}
