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

// shiftX/bob은 상자는 그대로 두고 도트 그림만 밀어낸다 (달려드는 타격 연출).
void DrawSpriteArt(HDC dc, const RECT& box, int kind, int alive, int flash, int bob, int shiftX);
void DrawPortrait(HDC dc, const RECT& box, int kind, int alive, int selected, int flash, int bob, int shiftX);

// 섹터를 판독하는 듯한 노이즈 연출. 주사위 판독과 볼륨 진입 화면이 함께 쓴다.
uint32_t Hash3(int a, int b, int c);
void DrawSectorStatic(HDC dc, const RECT& area, int die, int step, int level);
void DrawSectorHex(HDC dc, const RECT& area, int die, int step, int level);
void DrawScanlines(HDC dc, const RECT& area);
void DrawTornValue(HDC dc, const RECT& area, const wchar_t* value, COLORREF color, int die, int step, int level);

// 미판독 정보를 가리는 노이즈.
//   CorruptCode  ASCII 코드명을 길이 그대로 헥스·기호로 갈아 끼운다 (폭이 유지된다).
//   DrawHexBlock 사각형을 헥스 덤프로 채운다. 한글 설명문을 통째로 덮을 때 쓴다.
// 둘 다 step이 바뀌면 다시 섞이므로 살아 있는 노이즈처럼 보인다.
void CorruptCode(const wchar_t* source, wchar_t* out, int cap, int seed, int step);
void DrawHexBlock(HDC dc, const RECT& area, COLORREF color, int seed, int step, int rows);

// 화면 전체를 삼키는 노이즈. level은 0(깨끗)~1000(완전 백색소음)이고
// 그 값이 그대로 화면을 덮는 비율이 된다 (1000이면 빈틈이 없다).
void DrawScreenStatic(HDC dc, const RECT& area, int step, int level);
// 같은 노이즈를 가장자리 띠(thickness px) 안에만 그린다. 바깥 테두리가 가장
// 짙고 안쪽으로 갈수록 옅어져, 화면 중앙은 건드리지 않는다.
void DrawEdgeStatic(HDC dc, const RECT& area, int step, int level, int thickness);
// 잠식. 가장자리부터 노이즈가 화면을 갉아먹으며 안으로 좁혀 들어온다.
// eaten 0(멀쩡함) ~ 1000(남은 데가 없음). 경계는 매 프레임 흔들려 갉히는 것처럼 보인다.
void DrawCreepStatic(HDC dc, const RECT& area, int step, int eaten);
// 가장자리에서 안쪽으로 옅어지는 경고 테두리. 피격·위독 상태를 알린다.
void DrawEdgeGlow(HDC dc, const RECT& area, COLORREF color, int level, int thickness);
