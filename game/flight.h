#ifndef SKYLINER_FLIGHT_H
#define SKYLINER_FLIGHT_H

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif
