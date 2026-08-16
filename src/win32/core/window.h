#ifndef SKYLINER_WINDOW_H
#define SKYLINER_WINDOW_H

#include <windows.h>

int Window_Init(HINSTANCE hInstance);
HWND Window_GetHWND(void);
int Window_IsRunning(void);
void Window_ProcessMessages(void);

#endif
