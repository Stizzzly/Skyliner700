/* Desktop harness: identical software renderer and flight core, no Direct3D. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "soft3d.h"
#include "menu.h"
#include "framebuffer.h"
static Soft3D renderer;
static FlightModel flight;
static Pixel pixels[SKY_W*SKY_H];
static bool paused=true;
static MenuState menu;
static Pixel menuPixels[SKY_DISPLAY_W*SKY_DISPLAY_H];
static bool Key(int k){return (GetAsyncKeyState(k)&0x8000)!=0;}
static LRESULT CALLBACK Proc(HWND w,UINT msg,WPARAM a,LPARAM b){
    if(msg==WM_DESTROY){PostQuitMessage(0);return 0;}
    if(msg==WM_KILLFOCUS){menu.Pause();paused=true;SkyDrawMenu(menuPixels,SKY_DISPLAY_W,menu,pixels,renderer.Width());InvalidateRect(w,0,FALSE);}
    if(msg==WM_KEYDOWN&&!(b&(1L<<30))){
        if(a==VK_ESCAPE||a==VK_F10){DestroyWindow(w);return 0;}
        if(menu.Visible()){
            if(a==VK_UP)menu.Move(-1);
            if(a==VK_DOWN)menu.Move(1);
            if(a==VK_RETURN||a=='5'){
                MenuState::Action action=menu.Select();
                if(action==MenuState::Exit){DestroyWindow(w);return 0;}
                if(action==MenuState::Start){renderer.SetHalfResolution(menu.halfResolution);FlightModel_Reset(&flight);}
            }
            paused=menu.Visible();SkyDrawMenu(menuPixels,SKY_DISPLAY_W,menu,pixels,renderer.Width());InvalidateRect(w,0,FALSE);return 0;
        }
        if(a=='5'||a=='P'){menu.Pause();paused=true;SkyDrawMenu(menuPixels,SKY_DISPLAY_W,menu,pixels,renderer.Width());InvalidateRect(w,0,FALSE);}
        if(a=='0'||a=='R')FlightModel_Reset(&flight);
    }
    if(msg==WM_ERASEBKGND)return 1;
    if(msg==WM_PAINT){
        PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);RECT rc;GetClientRect(w,&rc);
        struct {BITMAPINFOHEADER head;DWORD masks[3];} bi;
        ZeroMemory(&bi,sizeof(bi));bi.head.biSize=sizeof(BITMAPINFOHEADER);bi.head.biWidth=SKY_DISPLAY_W;bi.head.biHeight=-SKY_DISPLAY_H;bi.head.biPlanes=1;bi.head.biBitCount=16;bi.head.biCompression=BI_BITFIELDS;
        bi.masks[0]=0xf800;bi.masks[1]=0x07e0;bi.masks[2]=0x001f;
        if(!menu.Visible()){if(renderer.HalfResolution())SkyScaleRect(pixels,renderer.Width(),menuPixels,SKY_DISPLAY_W,0,0,SKY_DISPLAY_W,SKY_DISPLAY_H);else SkyPresentRect(pixels,renderer.Width(),menuPixels,SKY_DISPLAY_W,0,0,SKY_DISPLAY_W,SKY_DISPLAY_H);}
        StretchDIBits(dc,0,0,rc.right,rc.bottom,0,0,bi.head.biWidth,-bi.head.biHeight,menuPixels,(BITMAPINFO*)&bi,DIB_RGB_COLORS,SRCCOPY);
        if(!menu.Visible()){
        SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));char text[128];
        sprintf(text,"SPD %d  ALT %d  THR %d%% %s",FxInt(flight.state.speed*Fix::Raw(235930)),FxInt(flight.state.altitude),FxInt(flight.state.throttle*100),paused?"PAUSE":"");
        TextOutA(dc,8,8,text,(int)strlen(text));const char* help="Arrows: fly | 1/3: gas | 7/9: yaw | 5: pause | 0: reset";TextOutA(dc,8,rc.bottom-23,help,(int)strlen(help));}
        EndPaint(w,&ps);return 0;
    }
    return DefWindowProc(w,msg,a,b);
}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE,LPSTR,int show){
    WNDCLASSA wc;ZeroMemory(&wc,sizeof(wc));wc.lpfnWndProc=Proc;wc.hInstance=instance;wc.lpszClassName="SkylinerS60Preview";wc.hCursor=LoadCursor(0,IDC_ARROW);RegisterClassA(&wc);
    HWND w=CreateWindowA(wc.lpszClassName,"Skyliner700 - Nokia 6600 software renderer preview",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,548,680,0,0,instance,0);
    if(!w)return 1;
    FlightModel_Reset(&flight);SkyDrawMenu(menuPixels,SKY_DISPLAY_W,menu,0,0);ShowWindow(w,show);
    DWORD last=GetTickCount(),accum=0;MSG msg;
    for(;;){while(PeekMessage(&msg,0,0,0,PM_REMOVE)){if(msg.message==WM_QUIT)return 0;TranslateMessage(&msg);DispatchMessage(&msg);}
        DWORD now=GetTickCount(),dt=now-last;if(dt<50){Sleep(1);continue;}last=now;if(dt>250)dt=250;
        FlightInput in={0};if(GetForegroundWindow()==w){in.pitch=(Key(VK_DOWN)?Fix::Raw(65536):0)-(Key(VK_UP)?Fix::Raw(65536):0);in.roll=(Key(VK_RIGHT)?Fix::Raw(65536):0)-(Key(VK_LEFT)?Fix::Raw(65536):0);in.throttle=(Key('3')?Fix::Raw(65536):0)-(Key('1')?Fix::Raw(65536):0);in.yaw=(Key('9')?Fix::Raw(65536):0)-(Key('7')?Fix::Raw(65536):0);}
        if(!paused){accum+=dt;while(accum>=20){FlightModel_Step(&flight,&in,Fix::Raw(1311));accum-=20;}}else accum=0;
        if(!paused){renderer.Render(pixels,renderer.Width(),flight.state);InvalidateRect(w,0,FALSE);}
    }
}
