#ifndef SKYLINER_FLIGHT_SCENARIO_H
#define SKYLINER_FLIGHT_SCENARIO_H

#include "flight.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FLIGHT_SCENARIO_IDLE,
    FLIGHT_SCENARIO_RUN,
    FLIGHT_SCENARIO_CLIMB,
    FLIGHT_SCENARIO_CIRCLE,
    FLIGHT_SCENARIO_RETURN_TURN_OUT,
    FLIGHT_SCENARIO_RETURN_LEG,
    FLIGHT_SCENARIO_RETURN_TURN_IN,
    FLIGHT_SCENARIO_APPROACH,
    FLIGHT_SCENARIO_CRUISE,
    FLIGHT_SCENARIO_DESCENT,
    FLIGHT_SCENARIO_LAND,
    FLIGHT_SCENARIO_PASS,
    FLIGHT_SCENARIO_FAIL
} FlightScenarioPhase;

typedef struct {
    FlightScenarioPhase phase;
    float phaseTime;
    float circleStartYaw;
    int circleCompleted;
    int telemetryEnabled;
    const char* failure;
} FlightScenario;

void FlightScenario_Init(FlightScenario* scenario);
void FlightScenario_Start(FlightScenario* scenario);
void FlightScenario_Cancel(FlightScenario* scenario);
void FlightScenario_BuildInput(const FlightScenario* scenario, const FlightState* state, FlightInput* input);
void FlightScenario_Observe(FlightScenario* scenario, const FlightState* state, float deltaTime);
int FlightScenario_IsActive(const FlightScenario* scenario);
const char* FlightScenario_Status(const FlightScenario* scenario);

#ifdef __cplusplus
}
#endif

#endif
