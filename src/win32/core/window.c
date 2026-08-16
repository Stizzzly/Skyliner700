// window.c — создание окна и цикл сообщений

#include <windows.h>
#include <stdio.h>
#include "render/renderer.h"

#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 600

static HWND g_hWnd = NULL;
static int g_running = 1;

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_DESTROY:
        case WM_CLOSE:
            g_running = 0;
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
}

int Window_Init(HINSTANCE hInstance) {
    WNDCLASS wc = {0};
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = L"SkyLinerWindowClass";
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClass(&wc)) {
        DWORD err = GetLastError();
        FILE* __f = fopen("C:\\Users\\ADMIN\\CLionProjects\\Skyliner700\\debug_init.log","a"); if(__f){ fprintf(__f, "RegisterClass failed: %lu\n", err); fclose(__f); }
        return 0;
    }

    g_hWnd = CreateWindowEx(
        0, L"SkyLinerWindowClass", L"SkyLiner 700 Flying Simulator",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, SCREEN_WIDTH, SCREEN_HEIGHT,
        NULL, NULL, hInstance, NULL
    );

    if (!g_hWnd) {
        DWORD err = GetLastError();
        FILE* __f = fopen("C:\\Users\\ADMIN\\CLionProjects\\Skyliner700\\debug_init.log","a"); if(__f){ fprintf(__f, "CreateWindowEx failed: %lu\n", err); fclose(__f); }
        return 0;
    }

    ShowWindow(g_hWnd, SW_SHOW);
    UpdateWindow(g_hWnd);

    return 1;
}

HWND Window_GetHWND() { return g_hWnd; }
int  Window_IsRunning() { return g_running; }
void Window_ProcessMessages() {
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}
