#ifndef SKYLINER_FLIGHT_INPUT_H
#define SKYLINER_FLIGHT_INPUT_H

#include "flight.h"

void Flight_ReadKeyboardInput(FlightInput* input);
int Flight_KeyPressed(int virtualKey);

#endif
