#ifndef SKYLINER_FLIGHT_H
#define SKYLINER_FLIGHT_H

typedef struct {
    float x, y, z;
    float pitch, yaw, roll;
    float speed;
    float throttle;
    int onGround;
} FlightState;

void Flight_Init(void);
void Flight_Update(float deltaTime);
void Flight_Reset(void);
const FlightState* Flight_GetState(void);

#endif
