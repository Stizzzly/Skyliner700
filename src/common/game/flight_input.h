#ifndef SKYLINER_FLIGHT_INPUT_H
#define SKYLINER_FLIGHT_INPUT_H

#include "flight.h"

void Flight_ReadKeyboardInput(FlightInput* input);
/* Poll menu and developer keys once per frame before querying KeyPressed. */
void Flight_UpdateInputFrame(void);
int Flight_KeyPressed(int virtualKey);

#endif
