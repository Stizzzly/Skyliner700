#include <d3d9.h>
#include <stdio.h>
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
    static const unsigned char d[7] = {30,17,17,17,17,17,30};
    static const unsigned char g[7] = {14,17,16,23,17,17,14};
    static const unsigned char h[7] = {17,17,17,31,17,17,17};
    static const unsigned char i[7] = {31,4,4,4,4,4,31};
    static const unsigned char k[7] = {17,18,20,24,20,18,17};
    static const unsigned char l[7] = {16,16,16,16,16,16,31};
    static const unsigned char m[7] = {17,27,21,21,17,17,17};
    static const unsigned char n[7] = {17,25,21,19,17,17,17};
    static const unsigned char p[7] = {30,17,17,30,16,16,16};
    static const unsigned char s[7] = {15,16,16,14,1,1,30};
    static const unsigned char t[7] = {31,4,4,4,4,4,4};
    if (character >= '0' && character <= '9') return g_digits[character - '0'];
    switch (character) {
        case '-': return minus; case '+': return plus; case 'A': return a;
        case 'B': return b; case 'D': return d; case 'G': return g;
        case 'H': return h; case 'I': return i; case 'K': return k; case 'L': return l;
        case 'M': return m; case 'N': return n; case 'P': return p;
        case 'S': return s; case 'T': return t; default: return blank;
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

static void AddText(HudVertex* vertices, int* count, float x, float y, const char* text, DWORD color) {
    const float pixel = 2.0f;
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
        x += 12.0f;
    }
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
