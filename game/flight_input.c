#include <windows.h>
#include <string.h>
#include "flight_input.h"

static int KeyDown(int key) { return (GetAsyncKeyState(key) & 0x8000) != 0; }

void Flight_ReadKeyboardInput(FlightInput* input) {
    if (!input) return;
    memset(input, 0, sizeof(*input));
    input->pitch = (KeyDown(VK_UP) ? 1.0f : 0.0f) - (KeyDown(VK_DOWN) ? 1.0f : 0.0f);
    /* The chase camera looks forward from behind the aircraft, so its screen
       horizontal axis is reversed from the world X axis.  Map arrows to the
       visible turn direction: Left banks/turns left, Right banks/turns right. */
    input->roll = (KeyDown(VK_LEFT) ? 1.0f : 0.0f) - (KeyDown(VK_RIGHT) ? 1.0f : 0.0f);
    input->yaw = (KeyDown('E') ? 1.0f : 0.0f) - (KeyDown('Q') ? 1.0f : 0.0f);
    input->throttle = (KeyDown(VK_SHIFT) ? 1.0f : 0.0f) - (KeyDown(VK_CONTROL) ? 1.0f : 0.0f);
    input->reset = KeyDown('R');
}

int Flight_KeyPressed(int virtualKey) { return (GetAsyncKeyState(virtualKey) & 1) != 0; }
