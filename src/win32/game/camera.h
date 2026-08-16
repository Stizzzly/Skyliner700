#ifndef SKYLINER_CAMERA_H
#define SKYLINER_CAMERA_H

#include "game/flight.h"

/* Camera mode is intentionally separate from the renderer: it only turns
 * player input and aircraft state into an eye/target pair for D3D9. */
void Camera_Init(void);
void Camera_Update(float deltaTime, const FlightState* flight);

#endif
