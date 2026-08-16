#include <d3d9.h>
#include <stdio.h>
#include <string.h>
#include "renderer.h"

/* A tiny 5x7 bitmap font rendered as coloured quads.  It keeps the HUD
 * entirely on the fixed function path: no D3DXFont, shaders or GDI overlay. */
typedef struct { float x, y, z, rhw; DWORD color; } HudVertex;

#define HUD_MAX_VERTICES 12288

static const unsigned char g_digits[10][7] = {
    {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31},
    {30,1,1,14,1,1,30}, {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8}, {14,17,17,14,17,17,14},
    {14,17,17,15,1,1,14}
};

static const unsigned char* GlyphFor(char character) {
    static const unsigned char blank[7] = {0,0,0,0,0,0,0};
    static const unsigned char minus[7] = {0,0,0,31,0,0,0};
    static const unsigned char plus[7]  = {0,4,4,31,4,4,0};
    static const unsigned char a[7] = {14,17,17,31,17,17,17};
    static const unsigned char b[7] = {30,17,17,30,17,17,30};
    static const unsigned char c[7] = {14,17,16,16,16,17,14};
    static const unsigned char d[7] = {30,17,17,17,17,17,30};
    static const unsigned char e[7] = {31,16,16,30,16,16,31};
    static const unsigned char f[7] = {31,16,16,30,16,16,16};
    static const unsigned char g[7] = {14,17,16,23,17,17,14};
    static const unsigned char h[7] = {17,17,17,31,17,17,17};
    static const unsigned char i[7] = {31,4,4,4,4,4,31};
    static const unsigned char k[7] = {17,18,20,24,20,18,17};
    static const unsigned char l[7] = {16,16,16,16,16,16,31};
    static const unsigned char m[7] = {17,27,21,21,17,17,17};
    static const unsigned char n[7] = {17,25,21,19,17,17,17};
    static const unsigned char p[7] = {30,17,17,30,16,16,16};
    static const unsigned char o[7] = {14,17,17,17,17,17,14};
    static const unsigned char r[7] = {30,17,17,30,20,18,17};
    static const unsigned char s[7] = {15,16,16,14,1,1,30};
    static const unsigned char t[7] = {31,4,4,4,4,4,4};
    static const unsigned char u[7] = {17,17,17,17,17,17,14};
    static const unsigned char v[7] = {17,17,17,17,17,10,4};
    static const unsigned char x[7] = {17,17,10,4,10,17,17};
    static const unsigned char y[7] = {17,17,10,4,4,4,4};
    if (character >= '0' && character <= '9') return g_digits[character - '0'];
    switch (character) {
        case '-': return minus; case '+': return plus; case 'A': return a;
        case 'B': return b; case 'C': return c; case 'D': return d; case 'E': return e;
        case 'F': return f; case 'G': return g;
        case 'H': return h; case 'I': return i; case 'K': return k; case 'L': return l;
        case 'M': return m; case 'N': return n; case 'O': return o; case 'P': return p;
        case 'R': return r; case 'S': return s; case 'T': return t; case 'U': return u;
        case 'V': return v; case 'X': return x; case 'Y': return y; default: return blank;
    }
}

static void AddQuad(HudVertex* vertices, int* count, float x0, float y0, float x1, float y1, DWORD color) {
    if (*count + 6 > HUD_MAX_VERTICES) return;
    vertices[(*count)++] = (HudVertex){x0,y0,0.0f,1.0f,color};
    vertices[(*count)++] = (HudVertex){x0,y1,0.0f,1.0f,color};
    vertices[(*count)++] = (HudVertex){x1,y1,0.0f,1.0f,color};
    vertices[(*count)++] = (HudVertex){x0,y0,0.0f,1.0f,color};
    vertices[(*count)++] = (HudVertex){x1,y1,0.0f,1.0f,color};
    vertices[(*count)++] = (HudVertex){x1,y0,0.0f,1.0f,color};
}

static void AddTextScaled(HudVertex* vertices, int* count, float x, float y, const char* text, DWORD color, float pixel) {
    for (int character = 0; text[character] != '\0'; ++character) {
        const unsigned char* glyph = GlyphFor(text[character]);
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                if (glyph[row] & (1 << (4 - column))) {
                    AddQuad(vertices, count, x + column * pixel, y + row * pixel,
                            x + (column + 1) * pixel, y + (row + 1) * pixel, color);
                }
            }
        }
        x += pixel * 6.0f;
    }
}

static void AddText(HudVertex* vertices, int* count, float x, float y, const char* text, DWORD color) {
    AddTextScaled(vertices, count, x, y, text, color, 2.0f);
}

void D3D9_RenderHud(float speedKph, float altitudeMeters, float pitchDegrees,
                    float bankDegrees, float headingDegrees) {
    IDirect3DDevice9* device = GetD3D9Device();
    HudVertex vertices[HUD_MAX_VERTICES];
    char speed[24], altitude[24], pitch[24], bank[24], heading[24];
    const DWORD panel = D3DCOLOR_ARGB(155, 4, 12, 20);
    const DWORD text = D3DCOLOR_ARGB(240, 220, 245, 255);
    int count = 0;
    if (!device) return;
    snprintf(speed, sizeof(speed), "SPD %03d KMH", (int)(speedKph + 0.5f));
    snprintf(altitude, sizeof(altitude), "ALT %04d M", (int)(altitudeMeters + 0.5f));
    snprintf(pitch, sizeof(pitch), "PIT %+03d", (int)(pitchDegrees + (pitchDegrees >= 0 ? 0.5f : -0.5f)));
    snprintf(bank, sizeof(bank), "BNK %+03d", (int)(bankDegrees + (bankDegrees >= 0 ? 0.5f : -0.5f)));
    snprintf(heading, sizeof(heading), "HDG %03d", (int)(headingDegrees + 0.5f));
    AddQuad(vertices, &count, 10.0f, 10.0f, 178.0f, 91.0f, panel);
    AddText(vertices, &count, 16.0f, 15.0f, speed, text);
    AddText(vertices, &count, 16.0f, 30.0f, altitude, text);
    AddText(vertices, &count, 16.0f, 45.0f, pitch, text);
    AddText(vertices, &count, 16.0f, 60.0f, bank, text);
    AddText(vertices, &count, 16.0f, 75.0f, heading, text);

    device->lpVtbl->SetTexture(device, 0, NULL);
    device->lpVtbl->SetFVF(device, D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    device->lpVtbl->SetTextureStageState(device,0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
    device->lpVtbl->SetTextureStageState(device,0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
    device->lpVtbl->SetTextureStageState(device,0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);
    device->lpVtbl->SetTextureStageState(device,0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);
    device->lpVtbl->SetRenderState(device,D3DRS_ZENABLE,FALSE);
    device->lpVtbl->SetRenderState(device,D3DRS_ZWRITEENABLE,FALSE);
    device->lpVtbl->SetRenderState(device,D3DRS_FOGENABLE,FALSE);
    device->lpVtbl->SetRenderState(device,D3DRS_ALPHABLENDENABLE,TRUE);
    device->lpVtbl->SetRenderState(device,D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
    device->lpVtbl->SetRenderState(device,D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
    device->lpVtbl->DrawPrimitiveUP(device,D3DPT_TRIANGLELIST,count / 3,vertices,sizeof(HudVertex));
    device->lpVtbl->SetRenderState(device,D3DRS_ALPHABLENDENABLE,FALSE);
    device->lpVtbl->SetRenderState(device,D3DRS_ZENABLE,TRUE);
    device->lpVtbl->SetRenderState(device,D3DRS_ZWRITEENABLE,TRUE);
}

void D3D9_RenderDevHud(const char* scenarioStatus, int telemetryEnabled,
                       float throttle, float verticalSpeed, float lift, float drag,
                       int onGround, float pitchInput, float rollInput, float yawInput) {
    IDirect3DDevice9* device = GetD3D9Device();
    HudVertex vertices[HUD_MAX_VERTICES];
    char throttleText[20], verticalText[20], liftText[20], dragText[20], groundText[20], inputText[20];
    const int showStatus = scenarioStatus && strcmp(scenarioStatus, "TST OFF") != 0;
    const DWORD panel = D3DCOLOR_ARGB(155, 4, 12, 20);
    const DWORD text = D3DCOLOR_ARGB(240, 255, 228, 170);
    int count = 0;
    float y = 105.0f;
    if (!device || (!showStatus && !telemetryEnabled)) return;
    AddQuad(vertices, &count, 10.0f, 100.0f, 226.0f, telemetryEnabled ? 213.0f : 125.0f, panel);
    if (showStatus) { AddText(vertices, &count, 16.0f, y, scenarioStatus, text); y += 15.0f; }
    if (telemetryEnabled) {
        snprintf(throttleText, sizeof(throttleText), "THR %03d", (int)(throttle * 100.0f + 0.5f));
        snprintf(verticalText, sizeof(verticalText), "VSP %+03d", (int)(verticalSpeed + (verticalSpeed >= 0 ? 0.5f : -0.5f)));
        snprintf(liftText, sizeof(liftText), "LFT %03d", (int)(lift + 0.5f));
        snprintf(dragText, sizeof(dragText), "DRG %03d", (int)(drag + 0.5f));
        snprintf(groundText, sizeof(groundText), "GND %d", onGround ? 1 : 0);
        snprintf(inputText, sizeof(inputText), "INP %+.0f%+.0f%+.0f", pitchInput, rollInput, yawInput);
        AddText(vertices, &count, 16.0f, y, throttleText, text); y += 15.0f;
        AddText(vertices, &count, 16.0f, y, verticalText, text); y += 15.0f;
        AddText(vertices, &count, 16.0f, y, liftText, text); y += 15.0f;
        AddText(vertices, &count, 16.0f, y, dragText, text); y += 15.0f;
        AddText(vertices, &count, 16.0f, y, groundText, text); y += 15.0f;
        AddText(vertices, &count, 16.0f, y, inputText, text);
    }
    device->lpVtbl->SetTexture(device, 0, NULL);
    device->lpVtbl->SetFVF(device, D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    device->lpVtbl->SetTextureStageState(device,0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
    device->lpVtbl->SetTextureStageState(device,0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
    device->lpVtbl->SetTextureStageState(device,0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);
    device->lpVtbl->SetTextureStageState(device,0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);
    device->lpVtbl->SetRenderState(device,D3DRS_ZENABLE,FALSE);
    device->lpVtbl->SetRenderState(device,D3DRS_ZWRITEENABLE,FALSE);
    device->lpVtbl->SetRenderState(device,D3DRS_FOGENABLE,FALSE);
    device->lpVtbl->SetRenderState(device,D3DRS_ALPHABLENDENABLE,TRUE);
    device->lpVtbl->SetRenderState(device,D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
    device->lpVtbl->SetRenderState(device,D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
    device->lpVtbl->DrawPrimitiveUP(device,D3DPT_TRIANGLELIST,count / 3,vertices,sizeof(HudVertex));
    device->lpVtbl->SetRenderState(device,D3DRS_ALPHABLENDENABLE,FALSE);
    device->lpVtbl->SetRenderState(device,D3DRS_ZENABLE,TRUE);
    device->lpVtbl->SetRenderState(device,D3DRS_ZWRITEENABLE,TRUE);
}

static float CenteredTextX(float screenWidth, const char* text, float pixel) {
    return (screenWidth - (float)strlen(text) * pixel * 6.0f) * 0.5f;
}

void D3D9_RenderMenu(int paused, int selection) {
    IDirect3DDevice9* device = GetD3D9Device();
    HudVertex vertices[HUD_MAX_VERTICES];
    D3DVIEWPORT9 viewport;
    const char* title = paused ? "PAUSED" : "SKYLINER 700";
    const char* items[3] = {"RESUME", "RETURN MENU", "EXIT GAME"};
    const int itemCount = paused ? 3 : 2;
    const DWORD dim = D3DCOLOR_ARGB(115, 0, 0, 0);
    const DWORD panel = D3DCOLOR_ARGB(225, 7, 20, 34);
    const DWORD border = D3DCOLOR_ARGB(255, 80, 184, 230);
    const DWORD regular = D3DCOLOR_ARGB(230, 220, 239, 250);
    const DWORD chosen = D3DCOLOR_ARGB(255, 255, 231, 122);
    const float panelWidth = 430.0f;
    const float panelHeight = paused ? 290.0f : 245.0f;
    float panelX, panelY, itemY;
    int count = 0;
    if (!device || FAILED(device->lpVtbl->GetViewport(device, &viewport))) return;
    if (!paused) { items[0] = "START FLIGHT"; items[1] = "EXIT GAME"; }
    if (selection < 0) selection = 0;
    if (selection >= itemCount) selection = itemCount - 1;
    panelX = ((float)viewport.Width - panelWidth) * 0.5f;
    panelY = ((float)viewport.Height - panelHeight) * 0.5f;

    AddQuad(vertices, &count, 0.0f, 0.0f, (float)viewport.Width, (float)viewport.Height, dim);
    AddQuad(vertices, &count, panelX, panelY, panelX + panelWidth, panelY + panelHeight, panel);
    AddQuad(vertices, &count, panelX, panelY, panelX + panelWidth, panelY + 3.0f, border);
    AddTextScaled(vertices, &count, CenteredTextX((float)viewport.Width, title, 4.0f),
                  panelY + 35.0f, title, border, 4.0f);
    itemY = panelY + 120.0f;
    for (int item = 0; item < itemCount; ++item) {
        const DWORD color = item == selection ? chosen : regular;
        if (item == selection) {
            AddQuad(vertices, &count, panelX + 42.0f, itemY - 7.0f,
                    panelX + panelWidth - 42.0f, itemY + 26.0f,
                    D3DCOLOR_ARGB(130, 35, 87, 114));
        }
        AddTextScaled(vertices, &count, CenteredTextX((float)viewport.Width, items[item], 3.0f),
                      itemY, items[item], color, 3.0f);
        itemY += 48.0f;
    }

    device->lpVtbl->SetTexture(device, 0, NULL);
    device->lpVtbl->SetFVF(device, D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    device->lpVtbl->SetTextureStageState(device,0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
    device->lpVtbl->SetTextureStageState(device,0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
    device->lpVtbl->SetTextureStageState(device,0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);
    device->lpVtbl->SetTextureStageState(device,0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);
    device->lpVtbl->SetRenderState(device,D3DRS_ZENABLE,FALSE);
    device->lpVtbl->SetRenderState(device,D3DRS_ZWRITEENABLE,FALSE);
    device->lpVtbl->SetRenderState(device,D3DRS_FOGENABLE,FALSE);
    device->lpVtbl->SetRenderState(device,D3DRS_ALPHABLENDENABLE,TRUE);
    device->lpVtbl->SetRenderState(device,D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
    device->lpVtbl->SetRenderState(device,D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
    device->lpVtbl->DrawPrimitiveUP(device,D3DPT_TRIANGLELIST,count / 3,vertices,sizeof(HudVertex));
    device->lpVtbl->SetRenderState(device,D3DRS_ALPHABLENDENABLE,FALSE);
    device->lpVtbl->SetRenderState(device,D3DRS_ZENABLE,TRUE);
    device->lpVtbl->SetRenderState(device,D3DRS_ZWRITEENABLE,TRUE);
}
