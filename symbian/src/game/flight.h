#include "fixed.h"
#ifndef SKYLINER_FLIGHT_H
#define SKYLINER_FLIGHT_H

/* Controls are normalised to [-1, 1], which makes the simulation independent
 * of Windows keyboard state and deterministic in command-line tests. */
typedef struct {
    Fix pitch;
    Fix roll;
    Fix yaw;
    Fix throttle;
    int reset;
} FlightInput;

typedef struct {
    Fix x, y, z;
    Fix pitch, yaw, roll;
    Fix speed;
    Fix throttle;
    Fix altitude;
    Fix verticalSpeed;
    Fix lift;
    Fix drag;
    /* Angles in radians.  They are kept in the state so deterministic tests
       and the dev HUD can inspect the actual aerodynamic input. */
    Fix angleOfAttack;
    Fix flightPathAngle;
    int onGround;
} FlightState;

typedef struct {
    FlightState state;
    Fix velocityX, velocityY, velocityZ;
} FlightModel;

void FlightModel_Reset(FlightModel* model);
void FlightModel_Step(FlightModel* model, const FlightInput* input, Fix deltaTime);
const FlightState* FlightModel_GetState(const FlightModel* model);

/* The game owns one player model; tests can create additional FlightModel
 * values and call the pure API above directly. */
void Flight_Init(void);
void Flight_Reset(void);
void Flight_Step(const FlightInput* input, Fix deltaTime);
const FlightState* Flight_GetState(void);

#endif
