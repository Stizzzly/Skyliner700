#include <windows.h>
#include <string.h>
#include "flight_input.h"

static int KeyDown(int key) { return (GetAsyncKeyState(key) & 0x8000) != 0; }
static unsigned char g_keyDown[256];
static unsigned char g_keyPressed[256];

void Flight_ReadKeyboardInput(FlightInput* input) {
    if (!input) return;
    memset(input, 0, sizeof(*input));
    input->pitch = (KeyDown(VK_UP) ? 1.0f : 0.0f) - (KeyDown(VK_DOWN) ? 1.0f : 0.0f);
    /* Keep the visual aircraft controls intuitive in the chase camera:
       Left rolls the left wing down and Right rolls the right wing down. */
    input->roll = (KeyDown(VK_RIGHT) ? 1.0f : 0.0f) - (KeyDown(VK_LEFT) ? 1.0f : 0.0f);
    input->yaw = (KeyDown('E') ? 1.0f : 0.0f) - (KeyDown('Q') ? 1.0f : 0.0f);
    input->throttle = (KeyDown(VK_SHIFT) ? 1.0f : 0.0f) - (KeyDown(VK_CONTROL) ? 1.0f : 0.0f);
    input->reset = KeyDown('R');
}

void Flight_UpdateInputFrame(void) {
    static const int trackedKeys[] = {VK_UP, VK_DOWN, VK_RETURN, VK_ESCAPE, VK_F5, VK_F6};
    for (int index = 0; index < (int)(sizeof(trackedKeys) / sizeof(trackedKeys[0])); ++index) {
        const int key = trackedKeys[index];
        const int isDown = KeyDown(key);
        g_keyPressed[key] = (unsigned char)(isDown && !g_keyDown[key]);
        g_keyDown[key] = (unsigned char)isDown;
    }
}

int Flight_KeyPressed(int virtualKey) { return g_keyPressed[virtualKey & 0xff] != 0; }
