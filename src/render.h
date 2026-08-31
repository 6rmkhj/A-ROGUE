#pragma once

#include <windows.h>
#include <stdint.h>
#include "game.h"

// GDI 위에 얹은 얇은 그리기 도구. 게임 규칙을 모르고, 넘겨받은 값만 그린다.
// 모든 그리기는 아래 고정 캔버스에 이뤄지고, 마지막에 창 크기로 확대된다.

static const int BASE_WIDTH = 1120, BASE_HEIGHT = 760;

static const COLORREF C_BG = RGB(8, 12, 17), C_PANEL = RGB(16, 23, 31), C_PANEL_2 = RGB(23, 33, 43);
static const COLORREF C_LINE = RGB(50, 71, 87), C_TEXT = RGB(218, 232, 238), C_DIM = RGB(120, 145, 157);
static const COLORREF C_GREEN = RGB(82, 231, 174), C_RED = RGB(255, 92, 82), C_YELLOW = RGB(255, 204, 75), C_BLUE = RGB(83, 170, 255);
static const COLORREF C_INK = RGB(6, 10, 15);

extern HFONT gFontSmall, gFontMedium, gFontLarge, gFontHuge;
void CreateRenderFonts();
void DestroyRenderFonts();

RECT MakeRect(int l, int t, int r, int b);
int Inside(const RECT& rect, int x, int y);
void ComputeCanvasTransform(int clientW, int clientH, float* scale, int* offsetX, int* offsetY);
POINT ScreenToCanvas(HWND window, int x, int y);

void Fill(HDC dc, const RECT& rect, COLORREF color);
void Outline(HDC dc, const RECT& rect, COLORREF color, int thickness);
void Panel(HDC dc, const RECT& rect, COLORREF fillColor, COLORREF borderColor);
void Text(HDC dc, int x, int y, const wchar_t* value, COLORREF color, HFONT font);
void TextRect(HDC dc, const RECT& rect, const wchar_t* value, COLORREF color, HFONT font, UINT flags);
void Bar(HDC dc, const RECT& rect, int value, int maximum, COLORREF color);
COLORREF MixColor(COLORREF from, COLORREF to, int amount);

void FormatFace(const Face* face, wchar_t* out);
COLORREF FaceColor(const Face* face);
void AppendStatus(wchar_t* output, const wchar_t* status);

void DrawSpriteArt(HDC dc, const RECT& box, int kind, int alive, int flash, int bob);
void DrawPortrait(HDC dc, const RECT& box, int kind, int alive, int selected, int flash, int bob);

// 섹터를 판독하는 듯한 노이즈 연출. 주사위 판독과 볼륨 진입 화면이 함께 쓴다.
uint32_t Hash3(int a, int b, int c);
void DrawSectorStatic(HDC dc, const RECT& area, int die, int step, int level);
void DrawSectorHex(HDC dc, const RECT& area, int die, int step, int level);
void DrawScanlines(HDC dc, const RECT& area);
void DrawTornValue(HDC dc, const RECT& area, const wchar_t* value, COLORREF color, int die, int step, int level);
